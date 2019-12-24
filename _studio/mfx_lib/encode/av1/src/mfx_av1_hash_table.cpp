// Copyright (c) 2019 Intel Corporation
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

#include "assert.h"
#include "numeric"
#include "algorithm"
#include "mfx_av1_defs.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_hash_table.h"

using namespace AV1Enc;

HashTable::HashTable()
    : m_blocks(nullptr)
    , m_numBlocksAllocated(0)
    , m_numBlocks(0)
{
}

HashTable::HashTable(HashTable &&ht)
    : m_blocks(ht.m_blocks)
    , m_numBlocksAllocated(ht.m_numBlocksAllocated)
    , m_numBlocks(ht.m_numBlocks)
{
    memcpy(m_bucketRange, ht.m_bucketRange, sizeof(m_bucketRange));
    ht.m_blocks = nullptr;
    ht.m_numBlocksAllocated = 0;
    ht.m_numBlocks = 0;
}
void HashTable::Free()
{
    if (m_blocks) {
        AV1_SafeFree(m_blocks);
        m_numBlocksAllocated = 0;
        m_numBlocks = 0;
    }
}

HashTable::~HashTable()
{
    Free();
}

void HashTable::Clear()
{
    memset(m_bucketRange, 0, sizeof(m_bucketRange));
}

void HashTable::Count(uint16_t hashIdx)
{
    ++m_bucketRange[int32_t(hashIdx)][0];
}

void HashTable::Alloc()
{
    int32_t offset = 0;
    for (int32_t i = 0; i < NUM_BUCKETS; i++) {
        const int32_t size = m_bucketRange[i][0];
        m_bucketRange[i][0] = offset;
        m_bucketRange[i][1] = offset;
        offset += size;
    }

    if (m_numBlocksAllocated < offset) {
        BlockHash *newBlocks = (BlockHash *)AV1_Malloc(offset * sizeof(BlockHash));
        ThrowIf(!newBlocks, std::bad_alloc());
        if (m_blocks)
            AV1_Free(m_blocks);
        m_blocks = newBlocks;
        m_numBlocksAllocated = offset;
    }
    m_numBlocks = offset;
}

int32_t HashTable::GetNumBlocks() const
{
    assert(m_numBlocksAllocated > 0);
    return m_numBlocks;
}

#if !ENABLE_HASH_MEM_REDUCTION
void HashTable::AddBlock(uint16_t hashIdx, uint16_t x, uint16_t y, uint16_t sum, BlockHash::Packed2Bytes srcSb)
#else
void HashTable::AddBlock(uint16_t hashIdx, uint16_t x, uint16_t y, BlockHash::Packed2Bytes srcSb)
#endif
{
    assert(m_numBlocksAllocated > 0);

    const int32_t index = m_bucketRange[int32_t(hashIdx)][1];
    BlockHash *b = m_blocks + index;
    b->srcSb = srcSb;
    b->y = y;
    b->x = x;
#if !ENABLE_HASH_MEM_REDUCTION
    b->sum = sum;
#endif
    m_bucketRange[int32_t(hashIdx)][1] = index + 1;
}

const BlockHash *HashTable::GetBucketBegin(uint16_t hashIdx) const
{
    assert(m_numBlocks >= m_bucketRange[int32_t(hashIdx)][0]);
    return m_blocks + m_bucketRange[hashIdx][0];
}

const BlockHash *HashTable::GetBucketEnd(uint16_t hashIdx) const
{
    assert(m_numBlocks >= m_bucketRange[int32_t(hashIdx)][1]);
    return m_blocks + m_bucketRange[int32_t(hashIdx)][1];
}

const int32_t HashTable::GetBucketSize(uint16_t hashIdx) const
{
    return ((hashIdx == 0xffff) ? m_numBlocks : m_bucketRange[hashIdx + 1][0]) - m_bucketRange[hashIdx][0];
}
#if ENABLE_HASH_MEM_REDUCTION
void AV1Enc::AllocTmpBuf(const TileBorders &tb, uint16_t **tmpBufForHash, int32_t *tmpBufForHashSize, uint16_t **workBufForHash)
#else
void AV1Enc::AllocTmpBuf(const TileBorders &tb, uint16_t **tmpBufForHash, int32_t *tmpBufForHashSize)
#endif
{
    const int32_t startX = tb.colStart * 8;
    const int32_t startY = tb.rowStart * 8;
    const int32_t width = (tb.colEnd - tb.colStart) * 8;
    const int32_t height = (tb.rowEnd - tb.rowStart) * 8;
    const int32_t alignedWidth = (width + 15) & ~15;
    const int32_t alignedHeight = (height + 15) & ~15;
    const int32_t accPitch = alignedWidth + 16;
#if ENABLE_HASH_MEM_REDUCTION
    const int32_t requiredTmpBufForHashSize = sizeof(uint16_t) * 1 * accPitch * (alignedHeight + 1);
    const int32_t requiredWorkBufForHashSize = sizeof(uint16_t) * 3 * accPitch * (HASH_WORK_LINES + 16 + 1);
#else
    const int32_t requiredTmpBufForHashSize = sizeof(uint16_t) * 4 * accPitch * (alignedHeight + 1);
#endif

    if (*tmpBufForHashSize < requiredTmpBufForHashSize) {
        *tmpBufForHash = (uint16_t *)AV1_Malloc(requiredTmpBufForHashSize);
        *tmpBufForHashSize = requiredTmpBufForHashSize;
    }
#if ENABLE_HASH_MEM_REDUCTION
    if (workBufForHash) {
        if (*workBufForHash == NULL) {
            *workBufForHash = (uint16_t *)AV1_Malloc(requiredWorkBufForHashSize);
        }
    }
#endif
}

static void get_2lines_of_4sums(const uint8_t *p0, const uint8_t *p1, int32_t len, uint16_t *out0, uint16_t *out1)
{
    __m256i b00, b16;
    __m256i in00, in01, in08, in09, in16, in17, in24;
    __m256i sum2_00, sum2_02, sum2_08, sum2_10, sum2_16;
    __m256i sum4_00, sum4_08;

    b00 = loada2_m128i(p0, p1);
    in00 = _mm256_unpacklo_epi8(b00, _mm256_setzero_si256());
    in08 = _mm256_unpackhi_epi8(b00, _mm256_setzero_si256());
    in01 = _mm256_alignr_epi8(in08, in00, 2);
    sum2_00 = _mm256_add_epi16(in00, in01);

    for (int32_t i = 0; i < len - 3; i += 16) {
        b16 = loada2_m128i(p0 + i + 16, p1 + i + 16);
        in16 = _mm256_unpacklo_epi8(b16, _mm256_setzero_si256());
        in24 = _mm256_unpackhi_epi8(b16, _mm256_setzero_si256());
        in09 = _mm256_alignr_epi8(in16, in08, 2);
        in17 = _mm256_alignr_epi8(in24, in16, 2);
        sum2_08 = _mm256_add_epi16(in08, in09);
        sum2_16 = _mm256_add_epi16(in16, in17);
        sum2_02 = _mm256_alignr_epi8(sum2_08, sum2_00, 4);
        sum2_10 = _mm256_alignr_epi8(sum2_16, sum2_08, 4);
        sum4_00 = _mm256_add_epi16(sum2_00, sum2_02);
        sum4_08 = _mm256_add_epi16(sum2_08, sum2_10);
        storea_si128(out0 + i + 0, si128_lo(sum4_00));
        storea_si128(out0 + i + 8, si128_lo(sum4_08));
        storea_si128(out1 + i + 0, si128_hi(sum4_00));
        storea_si128(out1 + i + 8, si128_hi(sum4_08));
        in08 = in24;
        sum2_00 = sum2_16;
    }
}


static void get_2lines_of_8sums_of_gradh(const uint8_t *p0, const uint8_t *p1, int32_t len, uint16_t *out0, uint16_t *out1)
{
    __m256i b00, b16, b32;
    __m256i in00, in01, in08, in09, in16, in17, in24, in25, in32, in33, in40;
    __m256i grad00, grad01, grad08, grad09, grad16, grad17, grad24, grad25, grad32;
    __m256i sum2_00, sum2_02, sum2_08, sum2_10, sum2_16, sum2_18, sum2_24;
    __m256i sum4_00, sum4_04, sum4_08, sum4_12, sum4_16;
    __m256i sum8_00, sum8_08;

    b00 = loada2_m128i(p0, p1);
    b16 = loada2_m128i(p0 + 16, p1 + 16);
    in00 = _mm256_unpacklo_epi8(b00, _mm256_setzero_si256());
    in08 = _mm256_unpackhi_epi8(b00, _mm256_setzero_si256());
    in16 = _mm256_unpacklo_epi8(b16, _mm256_setzero_si256());
    in24 = _mm256_unpackhi_epi8(b16, _mm256_setzero_si256());
    in01 = _mm256_alignr_epi8(in08, in00, 2);
    in09 = _mm256_alignr_epi8(in16, in08, 2);
    in17 = _mm256_alignr_epi8(in24, in16, 2);
    grad00 = _mm256_abs_epi16(_mm256_sub_epi16(in00, in01));
    grad08 = _mm256_abs_epi16(_mm256_sub_epi16(in08, in09));
    grad16 = _mm256_abs_epi16(_mm256_sub_epi16(in16, in17));
    grad01 = _mm256_alignr_epi8(grad08, grad00, 2);
    grad09 = _mm256_alignr_epi8(grad16, grad08, 2);
    sum2_00 = _mm256_add_epi16(grad00, grad01);
    sum2_08 = _mm256_add_epi16(grad08, grad09);
    sum2_02 = _mm256_alignr_epi8(sum2_08, sum2_00, 4);
    sum4_00 = _mm256_add_epi16(sum2_00, sum2_02);

    for (int32_t i = 0; i < len - 7; i += 16) {
        b32 = loada2_m128i(p0 + i + 32, p1 + i + 32);
        in32 = _mm256_unpacklo_epi8(b32, _mm256_setzero_si256());
        in40 = _mm256_unpackhi_epi8(b32, _mm256_setzero_si256());
        in25 = _mm256_alignr_epi8(in32, in24, 2);
        in33 = _mm256_alignr_epi8(in40, in32, 2);
        grad24 = _mm256_abs_epi16(_mm256_sub_epi16(in24, in25));
        grad32 = _mm256_abs_epi16(_mm256_sub_epi16(in32, in33));
        grad17 = _mm256_alignr_epi8(grad24, grad16, 2);
        grad25 = _mm256_alignr_epi8(grad32, grad24, 2);
        sum2_16 = _mm256_add_epi16(grad16, grad17);
        sum2_24 = _mm256_add_epi16(grad24, grad25);
        sum2_10 = _mm256_alignr_epi8(sum2_16, sum2_08, 4);
        sum2_18 = _mm256_alignr_epi8(sum2_24, sum2_16, 4);
        sum4_08 = _mm256_add_epi16(sum2_08, sum2_10);
        sum4_16 = _mm256_add_epi16(sum2_16, sum2_18);
        sum4_04 = _mm256_alignr_epi8(sum4_08, sum4_00, 8);
        sum4_12 = _mm256_alignr_epi8(sum4_16, sum4_08, 8);
        sum8_00 = _mm256_add_epi16(sum4_00, sum4_04);
        sum8_08 = _mm256_add_epi16(sum4_08, sum4_12);
        storea_si128(out0 + i + 0, si128_lo(sum8_00));
        storea_si128(out0 + i + 8, si128_lo(sum8_08));
        storea_si128(out1 + i + 0, si128_hi(sum8_00));
        storea_si128(out1 + i + 8, si128_hi(sum8_08));
        in24 = in40;
        grad16 = grad32;
        sum2_08 = sum2_24;
        sum4_00 = sum4_16;
    }
}

static void get_2lines_of_8sums_of_gradv(const uint8_t *p0, const uint8_t *p1, const uint8_t *p2, int32_t len, uint16_t *out0, uint16_t *out1)
{
    __m256i b00_row0, b00_row1, b16_row0, b16_row1, b32_row0, b32_row1;
    __m256i gradb00, gradb16, gradb32;
    __m256i grad00, grad01, grad08, grad09, grad16, grad17, grad24, grad25, grad32, grad40;
    __m256i sum2_00, sum2_02, sum2_08, sum2_10, sum2_16, sum2_18, sum2_24;
    __m256i sum4_00, sum4_04, sum4_08, sum4_12, sum4_16;
    __m256i sum8_00, sum8_08;

    b00_row0 = loada2_m128i(p0, p1);
    b00_row1 = loada2_m128i(p1, p2);
    b16_row0 = loada2_m128i(p0 + 16, p1 + 16);
    b16_row1 = loada2_m128i(p1 + 16, p2 + 16);
    gradb00 = _mm256_sub_epi8(_mm256_max_epu8(b00_row0, b00_row1), _mm256_min_epu8(b00_row0, b00_row1));
    gradb16 = _mm256_sub_epi8(_mm256_max_epu8(b16_row0, b16_row1), _mm256_min_epu8(b16_row0, b16_row1));
    grad00 = _mm256_unpacklo_epi8(gradb00, _mm256_setzero_si256());
    grad08 = _mm256_unpackhi_epi8(gradb00, _mm256_setzero_si256());
    grad16 = _mm256_unpacklo_epi8(gradb16, _mm256_setzero_si256());
    grad24 = _mm256_unpackhi_epi8(gradb16, _mm256_setzero_si256());
    grad01 = _mm256_alignr_epi8(grad08, grad00, 2);
    grad09 = _mm256_alignr_epi8(grad16, grad08, 2);
    sum2_00 = _mm256_add_epi16(grad00, grad01);
    sum2_08 = _mm256_add_epi16(grad08, grad09);
    sum2_02 = _mm256_alignr_epi8(sum2_08, sum2_00, 4);
    sum4_00 = _mm256_add_epi16(sum2_00, sum2_02);

    for (int32_t i = 0; i < len - 7; i += 16) {
        b32_row0 = loada2_m128i(p0 + i + 32, p1 + i + 32);
        b32_row1 = loada2_m128i(p1 + i + 32, p2 + i + 32);
        gradb32 = _mm256_sub_epi8(_mm256_max_epu8(b32_row0, b32_row1), _mm256_min_epu8(b32_row0, b32_row1));
        grad32 = _mm256_unpacklo_epi8(gradb32, _mm256_setzero_si256());
        grad40 = _mm256_unpackhi_epi8(gradb32, _mm256_setzero_si256());
        grad17 = _mm256_alignr_epi8(grad24, grad16, 2);
        grad25 = _mm256_alignr_epi8(grad32, grad24, 2);
        sum2_16 = _mm256_add_epi16(grad16, grad17);
        sum2_24 = _mm256_add_epi16(grad24, grad25);
        sum2_10 = _mm256_alignr_epi8(sum2_16, sum2_08, 4);
        sum2_18 = _mm256_alignr_epi8(sum2_24, sum2_16, 4);
        sum4_08 = _mm256_add_epi16(sum2_08, sum2_10);
        sum4_16 = _mm256_add_epi16(sum2_16, sum2_18);
        sum4_04 = _mm256_alignr_epi8(sum4_08, sum4_00, 8);
        sum4_12 = _mm256_alignr_epi8(sum4_16, sum4_08, 8);
        sum8_00 = _mm256_add_epi16(sum4_00, sum4_04);
        sum8_08 = _mm256_add_epi16(sum4_08, sum4_12);
        storea_si128(out0 + i + 0, si128_lo(sum8_00));
        storea_si128(out0 + i + 8, si128_lo(sum8_08));
        storea_si128(out1 + i + 0, si128_hi(sum8_00));
        storea_si128(out1 + i + 8, si128_hi(sum8_08));
        grad16 = grad32;
        grad24 = grad40;
        sum2_08 = sum2_24;
        sum4_00 = sum4_16;
    }
}

static void PrintHashStat(const int32_t bucketSizes[65536], int32_t blockSize)
{
    int32_t total = std::accumulate(bucketSizes, bucketSizes + 65536, 0);
    fprintf(stderr, "%d\n", total);
    std::vector<std::pair<uint16_t, uint32_t>> vec_of_pairs_of_hash_and_size(1 << 16);
    for (uint32_t hash = 0; hash < (1 << 16); hash++)
        vec_of_pairs_of_hash_and_size[hash] = std::make_pair((uint16_t)hash, bucketSizes[hash]);
    std::sort(
        vec_of_pairs_of_hash_and_size.begin(),
        vec_of_pairs_of_hash_and_size.end(),
        [](auto p0, auto p1) { return p0.second > p1.second; });

    int32_t subtotal = 0;
    for (int32_t i = 0; i < 50; i++)
        subtotal += vec_of_pairs_of_hash_and_size[i].second;
    fprintf(stderr, "%dx%d: 50 top buckets (%8d / %8d = %.2f%%)\n", blockSize, blockSize, subtotal, total, 100.f * subtotal / total);
    for (int32_t i = 0; i < 50; i++) {
        const uint16_t hash = vec_of_pairs_of_hash_and_size[i].first;
        const int32_t grad = hash >> 12;
        const int32_t dc0 = (hash >> 9) & 7;
        const int32_t dc1 = (hash >> 6) & 7;
        const int32_t dc2 = (hash >> 3) & 7;
        const int32_t dc3 = (hash >> 0) & 7;
        const int32_t size = vec_of_pairs_of_hash_and_size[i].second;
        fprintf(stderr, "[%2d %d %d %d %d]: %8d / %8d = %.2f%%\n", grad, dc0, dc1, dc2, dc3, size, total, 100.f * size / total);
    }
    fprintf(stderr, "\n");
}

void AV1Enc::BuildHashTable_avx2(const FrameData &frameData, const TileBorders &tileBorderMi, uint16_t *tmpBufForHash,
                                 HashTable &hashTable8, std::vector<AlignedBlockHash> &hashIndexes8
#if !ENABLE_ONLY_HASH8
                                 , HashTable &hashTable16, std::vector<AlignedBlockHash> &hashIndexes16
#elif ENABLE_HASH_MEM_REDUCTION
                                , uint16_t *workBufForHash
#endif
                                 )
{
    const int32_t startY = tileBorderMi.rowStart * 8;
    const int32_t startX = tileBorderMi.colStart * 8;
    const int32_t tileWidth = (tileBorderMi.colEnd - tileBorderMi.colStart) * 8;
    const int32_t tileHeight = (tileBorderMi.rowEnd - tileBorderMi.rowStart) * 8;
    const int32_t alignedTileWidth = (tileWidth + 15) & ~15;
    const int32_t alignedTileHeight = (tileHeight + 15) & ~15;
    const int32_t accPitch = alignedTileWidth + 16;
#if ENABLE_HASH_MEM_REDUCTION
    uint16_t *acc = workBufForHash;
    uint16_t *accGradH = acc + accPitch * (alignedTileHeight + 1);
    uint16_t *accGradV = accGradH + accPitch * (alignedTileHeight + 1);
    uint16_t *hashStore = tmpBufForHash;
#else
    uint16_t *acc = tmpBufForHash;
    uint16_t *accGradH = acc + accPitch * (alignedTileHeight + 1);
    uint16_t *accGradV = accGradH + accPitch * (alignedTileHeight + 1);
    uint16_t *hashStore = accGradV + accPitch * (alignedTileHeight + 1);
#endif

    const int32_t hashIndexes8Pitch = frameData.width >> 3;
    const int32_t hashIndexes16Pitch = frameData.width >> 4;

    const int32_t frmPitch = frameData.pitch_luma_pix;
    const uint8_t *frm = frameData.y + startX + startY * frmPitch;

    // horizontal addition
    for (int32_t y = 0; y < tileHeight; y += 2, frm += 2 * frmPitch) {
        get_2lines_of_4sums(frm, frm + frmPitch, tileWidth, acc + (y + 0) * accPitch, acc + (y + 1) * accPitch);
        get_2lines_of_8sums_of_gradh(frm, frm + 1 * frmPitch, tileWidth, accGradH + (y + 0) * accPitch, accGradH + (y + 1) * accPitch);
        get_2lines_of_8sums_of_gradv(frm, frm + 1 * frmPitch, frm + 2 * frmPitch, tileWidth, accGradV + (y + 0) * accPitch, accGradV + (y + 1) * accPitch);
    }

    // vertical addition
    for (int32_t x = 0; x < tileWidth - 3; x += 16) {
        __m256i dc, dc0, gradh, gradh0, gradv, gradv0;
        dc0 = gradh0 = gradv0 = _mm256_setzero_si256();
        dc = _mm256_add_epi16(loada_si256(acc + 1 * accPitch + x), loada_si256(acc + 0 * accPitch + x));
        dc = _mm256_add_epi16(loada_si256(acc + 2 * accPitch + x), dc);
        gradh = _mm256_add_epi16(loada_si256(accGradH + 1 * accPitch + x), loada_si256(accGradH + 0 * accPitch + x));
        gradh = _mm256_add_epi16(loada_si256(accGradH + 2 * accPitch + x), gradh);
        gradh = _mm256_add_epi16(loada_si256(accGradH + 3 * accPitch + x), gradh);
        gradh = _mm256_add_epi16(loada_si256(accGradH + 4 * accPitch + x), gradh);
        gradh = _mm256_add_epi16(loada_si256(accGradH + 5 * accPitch + x), gradh);
        gradh = _mm256_add_epi16(loada_si256(accGradH + 6 * accPitch + x), gradh);
        gradv = _mm256_add_epi16(loada_si256(accGradV + 1 * accPitch + x), loada_si256(accGradV + 0 * accPitch + x));
        gradv = _mm256_add_epi16(loada_si256(accGradV + 2 * accPitch + x), gradv);
        gradv = _mm256_add_epi16(loada_si256(accGradV + 3 * accPitch + x), gradv);
        gradv = _mm256_add_epi16(loada_si256(accGradV + 4 * accPitch + x), gradv);
        gradv = _mm256_add_epi16(loada_si256(accGradV + 5 * accPitch + x), gradv);
        gradv = _mm256_add_epi16(loada_si256(accGradV + 6 * accPitch + x), gradv);

        for (int32_t y = 0; y < tileHeight - 3; y += 2) {
            dc = _mm256_sub_epi16(dc, dc0);
            dc = _mm256_add_epi16(loada_si256(acc + (y + 3) * accPitch + x), dc);
            dc0 = loada_si256(acc + (y + 0) * accPitch + x);
            storea_si256(acc + (y + 0) * accPitch + x, dc);
            dc = _mm256_sub_epi16(dc, dc0);
            dc = _mm256_add_epi16(loada_si256(acc + (y + 4) * accPitch + x), dc);
            dc0 = loada_si256(acc + (y + 1) * accPitch + x);
            storea_si256(acc + (y + 1) * accPitch + x, dc);

            gradh = _mm256_sub_epi16(gradh, gradh0);
            gradh = _mm256_add_epi16(loada_si256(accGradH + (y + 7) * accPitch + x), gradh);
            gradh0 = loada_si256(accGradH + (y + 0) * accPitch + x);
            storea_si256(accGradH + (y + 0) * accPitch + x, gradh);
            gradh = _mm256_sub_epi16(gradh, gradh0);
            gradh = _mm256_add_epi16(loada_si256(accGradH + (y + 8) * accPitch + x), gradh);
            gradh0 = loada_si256(accGradH + (y + 1) * accPitch + x);
            storea_si256(accGradH + (y + 1) * accPitch + x, gradh);

            gradv = _mm256_sub_epi16(gradv, gradv0);
            gradv = _mm256_add_epi16(loada_si256(accGradV + (y + 7) * accPitch + x), gradv);
            gradv0 = loada_si256(accGradV + (y + 0) * accPitch + x);
            storea_si256(accGradV + (y + 0) * accPitch + x, gradv);
            gradv = _mm256_sub_epi16(gradv, gradv0);
            gradv = _mm256_add_epi16(loada_si256(accGradV + (y + 8) * accPitch + x), gradv);
            gradv0 = loada_si256(accGradV + (y + 1) * accPitch + x);
            storea_si256(accGradV + (y + 1) * accPitch + x, gradv);
        }
    }

    hashTable8.Clear();

    // 8x8 hashes
    for (int32_t y = 0; y < tileHeight - 7; y++) {
        const __m256i maskAligned = (y & 7) ? _mm256_setzero_si256() : _mm256_setr_epi64x(0xffff, 0, 0xffff, 0);
        for (int32_t x = 0; x < tileWidth - 7; x += 16) {
            uint16_t *hashBuf = hashStore + y * accPitch + x;

            __m256i dc0, dc1, dc2, dc3, dc8x8, gradh, gradv, grad, hsh, nonZeroGrad, skipIbc;
            dc0 = loada_si256(acc + (y + 0) * accPitch + x + 0);
            dc1 = loadu_si256(acc + (y + 0) * accPitch + x + 4);
            dc2 = loada_si256(acc + (y + 4) * accPitch + x + 0);
            dc3 = loadu_si256(acc + (y + 4) * accPitch + x + 4);
            dc8x8 = _mm256_add_epi16(_mm256_add_epi16(dc0, dc1), _mm256_add_epi16(dc2, dc3));
            storea_si256(acc + y * accPitch + x + 0, dc8x8);
            gradh = loada_si256(accGradH + y * accPitch + x);
            gradv = loada_si256(accGradV + y * accPitch + x);
            grad = _mm256_adds_epu16(gradh, gradv);
            nonZeroGrad = _mm256_cmpgt_epi16(gradh, _mm256_setzero_si256());
            nonZeroGrad = _mm256_and_si256(nonZeroGrad, _mm256_cmpgt_epi16(gradv, _mm256_setzero_si256()));

            storea_si256(accGradH + y * accPitch + x, _mm256_add_epi16(
                                     _mm256_add_epi16(gradh,
                                                      loadu_si256(accGradH + (y + 0) * accPitch + x + 8)),
                                     _mm256_add_epi16(loada_si256(accGradH + (y + 8) * accPitch + x + 0),
                                                      loadu_si256(accGradH + (y + 8) * accPitch + x + 8))));
            storea_si256(accGradV + y * accPitch + x, _mm256_add_epi16(
                                     _mm256_add_epi16(gradv,
                                                      loadu_si256(accGradV + (y + 0) * accPitch + x + 8)),
                                     _mm256_add_epi16(loada_si256(accGradV + (y + 8) * accPitch + x + 0),
                                                      loadu_si256(accGradV + (y + 8) * accPitch + x + 8))));

            grad = _mm256_slli_epi16(grad, 1);
            grad = _mm256_adds_epu16(grad, grad);
            grad = _mm256_srli_epi16(grad, (8) + 4);
            dc0 = _mm256_srli_epi16(dc0, (6 - 2) + 5);
            dc1 = _mm256_srli_epi16(dc1, (6 - 2) + 5);
            dc2 = _mm256_srli_epi16(dc2, (6 - 2) + 5);
            dc3 = _mm256_srli_epi16(dc3, (6 - 2) + 5);

            skipIbc = _mm256_cmpeq_epi16(grad, _mm256_setzero_si256());
            skipIbc = _mm256_and_si256(skipIbc, _mm256_cmpeq_epi16(dc0, dc1));
            skipIbc = _mm256_and_si256(skipIbc, _mm256_cmpeq_epi16(dc0, dc2));
            skipIbc = _mm256_and_si256(skipIbc, _mm256_cmpeq_epi16(dc0, dc3));

            grad = _mm256_slli_epi16(grad, 12);
            dc0 = _mm256_slli_epi16(dc0, 9);
            dc1 = _mm256_slli_epi16(dc1, 6);
            dc2 = _mm256_slli_epi16(dc2, 3);
            hsh = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_or_si256(grad, dc0), dc1), dc2), dc3);

            nonZeroGrad = _mm256_or_si256(nonZeroGrad, maskAligned);
            nonZeroGrad = _mm256_andnot_si256(skipIbc, nonZeroGrad);
            hsh = _mm256_and_si256(hsh, nonZeroGrad);

            storea_si256(hashBuf, hsh);

            const int32_t numHashes = std::min(16, tileWidth - 7 - x);
            if ((y & 7) == 0) {
                const int32_t alignedBlockIdx = ((y + startY) >> 3) * hashIndexes8Pitch + ((x + startX) >> 3);
                hashIndexes8[alignedBlockIdx].hash = hashBuf[0];
#if !ENABLE_HASH_MEM_REDUCTION
                hashIndexes8[alignedBlockIdx].sum = acc[y * accPitch + x];
#endif
                if (numHashes >= 8) {
                    hashIndexes8[alignedBlockIdx + 1].hash = hashBuf[8];
#if !ENABLE_HASH_MEM_REDUCTION
                    hashIndexes8[alignedBlockIdx + 1].sum = acc[y * accPitch + x + 8];
#endif
                }
            }
        }
    }

    assert(tileWidth % 8 == 0);
    uint16_t *ph = hashStore;
    for (int32_t y = 0; y < tileHeight - 7; y++, ph += accPitch) {
        int32_t x = 0;
        for (; x < tileWidth - 8; x += 8) {
            if (ph[x + 0]) hashTable8.Count(ph[x + 0]);
            if (ph[x + 1]) hashTable8.Count(ph[x + 1]);
            if (ph[x + 2]) hashTable8.Count(ph[x + 2]);
            if (ph[x + 3]) hashTable8.Count(ph[x + 3]);
            if (ph[x + 4]) hashTable8.Count(ph[x + 4]);
            if (ph[x + 5]) hashTable8.Count(ph[x + 5]);
            if (ph[x + 6]) hashTable8.Count(ph[x + 6]);
            if (ph[x + 7]) hashTable8.Count(ph[x + 7]);
        }
        if (ph[x]) hashTable8.Count(ph[x]);
    }

    //PrintHashStat(hashTable8.m_bucketBegin, 8);

    // reserve space for each bucket
    hashTable8.Alloc();

#if !ENABLE_ONLY_HASH8
    hashTable16.Clear();

    // 16x16 hashes
    for (int32_t y = 0; y < tileHeight - 15; y++) {
        const __m256i maskAligned = (y & 15) ? _mm256_setzero_si256() : _mm256_setr_epi64x(0xffff, 0, 0, 0);
        for (int32_t x = 0; x < tileWidth - 15; x += 16) {

            uint16_t *hashBuf = accGradV + y * accPitch + x;

            __m256i dc0, dc1, dc2, dc3, dc16x16, gradh, gradv, grad, hsh, nonZeroGrad, skipIbc;
            dc0 = loada_si256(acc + (y + 0) * accPitch + x + 0);
            dc1 = loadu_si256(acc + (y + 0) * accPitch + x + 8);
            dc2 = loada_si256(acc + (y + 8) * accPitch + x + 0);
            dc3 = loadu_si256(acc + (y + 8) * accPitch + x + 8);
            dc16x16 = _mm256_add_epi16(_mm256_add_epi16(dc0, dc1), _mm256_add_epi16(dc2, dc3));
            gradh = loada_si256(accGradH + (y + 0) * accPitch + x + 0);
            gradv = loada_si256(accGradV + (y + 0) * accPitch + x + 0);
            storea_si256(accGradH + y * accPitch + x, dc16x16);
            grad = _mm256_adds_epu16(gradh, gradv);
            nonZeroGrad = _mm256_cmpgt_epi16(gradh, _mm256_setzero_si256());
            nonZeroGrad = _mm256_and_si256(nonZeroGrad, _mm256_cmpgt_epi16(gradv, _mm256_setzero_si256()));

            grad = _mm256_srli_epi16(grad, (8 + 0) + 4);
            dc0 = _mm256_srli_epi16(dc0, (8 - 2) + 5);
            dc1 = _mm256_srli_epi16(dc1, (8 - 2) + 5);
            dc2 = _mm256_srli_epi16(dc2, (8 - 2) + 5);
            dc3 = _mm256_srli_epi16(dc3, (8 - 2) + 5);

            skipIbc = _mm256_cmpeq_epi16(grad, _mm256_setzero_si256());
            skipIbc = _mm256_and_si256(skipIbc, _mm256_cmpeq_epi16(dc0, dc1));
            skipIbc = _mm256_and_si256(skipIbc, _mm256_cmpeq_epi16(dc0, dc2));
            skipIbc = _mm256_and_si256(skipIbc, _mm256_cmpeq_epi16(dc0, dc3));

            grad = _mm256_slli_epi16(grad, 12);
            dc0 = _mm256_slli_epi16(dc0, 9);
            dc1 = _mm256_slli_epi16(dc1, 6);
            dc2 = _mm256_slli_epi16(dc2, 3);
            hsh = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_or_si256(grad, dc0), dc1), dc2), dc3);

            nonZeroGrad = _mm256_or_si256(nonZeroGrad, maskAligned);
            nonZeroGrad = _mm256_andnot_si256(skipIbc, nonZeroGrad);
            hsh = _mm256_and_si256(hsh, nonZeroGrad);

            storea_si256(hashBuf, hsh);

            if ((y & 15) == 0) {
                const int32_t alignedBlockIdx = ((y + startY) >> 4) * hashIndexes16Pitch + ((x + startX) >> 4);
                hashIndexes16[alignedBlockIdx].hash = hashBuf[0];
                hashIndexes16[alignedBlockIdx].sum = accGradH[y * accPitch + x];
            }
        }
    }

    assert(tileWidth % 8 == 0);
    ph = accGradV;
    for (int32_t y = 0; y < tileHeight - 15; y++, ph += accPitch) {
        int32_t x = 0;
        for (; x < tileWidth - 16; x += 8) {
            if (ph[x + 0]) hashTable16.Count(ph[x + 0]);
            if (ph[x + 1]) hashTable16.Count(ph[x + 1]);
            if (ph[x + 2]) hashTable16.Count(ph[x + 2]);
            if (ph[x + 3]) hashTable16.Count(ph[x + 3]);
            if (ph[x + 4]) hashTable16.Count(ph[x + 4]);
            if (ph[x + 5]) hashTable16.Count(ph[x + 5]);
            if (ph[x + 6]) hashTable16.Count(ph[x + 6]);
            if (ph[x + 7]) hashTable16.Count(ph[x + 7]);
        }
        if (ph[x]) hashTable16.Count(ph[x]);
    }

    //PrintHashStat(hashTable16.m_bucketBegin, 16);

    // reserve space for each bucket
    hashTable16.Alloc();
#endif

    //// re-compute hash indexes and store them
    //uint16_t *phashStore = hashStore - startX - startY * accPitch;
    //uint16_t *pacc = acc - startX - startY * accPitch;
    //int32_t endY = startY + tileHeight - 7;
    //int32_t endX = startX + tileWidth - 7;
    //for (int32_t sby = startY - 7; sby < endY; sby += 64) {
    //    for (int32_t sbx = startX - 7; sbx < endX; sbx += 64) {
    //        const BlockHash::Packed2Bytes srcSb = { uint8_t((sbx + 7) >> 6), uint8_t((sby + 7) >> 6) };
    //        const int32_t sbEndY = std::min(sby + 64, endY);
    //        const int32_t sbEndX = std::min(sbx + 64, endX);
    //        for (int32_t y = std::max(sby, startY); y < sbEndY; y++)
    //            for (int32_t x = std::max(sbx, startX); x < sbEndX; x++)
    //                if (phashStore[y * accPitch + x])
    //                    hashTable8.AddBlock(phashStore[y * accPitch + x], (uint16_t)x, (uint16_t)y, pacc[y * accPitch + x], srcSb);
    //    }
    //}

    //// re-compute hash indexes and store them
    //phashStore = accGradV - startX - startY * accPitch;
    //pacc = accGradH - startX - startY * accPitch;
    //endY = startY + tileHeight - 15;
    //endX = startX + tileWidth - 15;
    //for (int32_t sby = startY - 15; sby < endY; sby += 64) {
    //    for (int32_t sbx = startX - 15; sbx < endX; sbx += 64) {
    //        const BlockHash::Packed2Bytes srcSb = { uint8_t((sbx + 15) >> 6), uint8_t((sby + 15) >> 6) };
    //        const int32_t sbEndY = std::min(sby + 64, endY);
    //        const int32_t sbEndX = std::min(sbx + 64, endX);
    //        for (int32_t y = std::max(sby, startY); y < sbEndY; y++)
    //            for (int32_t x = std::max(sbx, startX); x < sbEndX; x++)
    //                if (phashStore[y * accPitch + x])
    //                    hashTable16.AddBlock(phashStore[y * accPitch + x], (uint16_t)x, (uint16_t)y, pacc[y * accPitch + x], srcSb);
    //    }
    //}
}


#if ENABLE_HASH_MEM_REDUCTION
void AV1Enc::BuildHashTable_avx2_lowmem(const FrameData &frameData, const TileBorders &tileBorderMi, uint16_t *tmpBufForHash,
    HashTable &hashTable8, std::vector<AlignedBlockHash> &hashIndexes8, uint16_t *workBufForHash)
{
    const int32_t startY = tileBorderMi.rowStart * 8;
    const int32_t startX = tileBorderMi.colStart * 8;
    const int32_t tileWidth = (tileBorderMi.colEnd - tileBorderMi.colStart) * 8;
    const int32_t TrueTileHeight = (tileBorderMi.rowEnd - tileBorderMi.rowStart) * 8;
    const int32_t alignedTileWidth = (tileWidth + 15) & ~15;
    const int32_t alignedTileHeight = (TrueTileHeight + 15) & ~15;
    const int32_t accPitch = alignedTileWidth + 16;

    uint16_t *acc = workBufForHash;
    uint16_t *accGradH = acc + accPitch * (HASH_WORK_LINES + 16 + 1);
    uint16_t *accGradV = accGradH + accPitch * (HASH_WORK_LINES + 16 + 1);
    uint16_t *hashStore = tmpBufForHash;

    const int32_t hashIndexes8Pitch = frameData.width >> 3;
    const int32_t hashIndexes16Pitch = frameData.width >> 4;

    const int32_t frmPitch = frameData.pitch_luma_pix;
    //const uint8_t *frm = frameData.y + startX + startY * frmPitch;

    hashTable8.Clear();

    // horizontal addition
    for (int32_t tileHeight = 0; tileHeight < TrueTileHeight; tileHeight += HASH_WORK_LINES) {

        uint8_t *frm = frameData.y + startX + (startY+tileHeight) * frmPitch;

        for (int32_t y = 0; y < MIN(TrueTileHeight - tileHeight, HASH_WORK_LINES + 16); y += 2, frm += 2 * frmPitch) {
            get_2lines_of_4sums(frm, frm + frmPitch, tileWidth, acc + (y + 0) * accPitch, acc + (y + 1) * accPitch);
            get_2lines_of_8sums_of_gradh(frm, frm + 1 * frmPitch, tileWidth, accGradH + (y + 0) * accPitch, accGradH + (y + 1) * accPitch);
            get_2lines_of_8sums_of_gradv(frm, frm + 1 * frmPitch, frm + 2 * frmPitch, tileWidth, accGradV + (y + 0) * accPitch, accGradV + (y + 1) * accPitch);
        }

        // vertical addition
        for (int32_t x = 0; x < tileWidth - 3; x += 16) {
            __m256i dc, dc0, gradh, gradh0, gradv, gradv0;
            dc0 = gradh0 = gradv0 = _mm256_setzero_si256();
            dc = _mm256_add_epi16(loada_si256(acc + 1 * accPitch + x), loada_si256(acc + 0 * accPitch + x));
            dc = _mm256_add_epi16(loada_si256(acc + 2 * accPitch + x), dc);
            gradh = _mm256_add_epi16(loada_si256(accGradH + 1 * accPitch + x), loada_si256(accGradH + 0 * accPitch + x));
            gradh = _mm256_add_epi16(loada_si256(accGradH + 2 * accPitch + x), gradh);
            gradh = _mm256_add_epi16(loada_si256(accGradH + 3 * accPitch + x), gradh);
            gradh = _mm256_add_epi16(loada_si256(accGradH + 4 * accPitch + x), gradh);
            gradh = _mm256_add_epi16(loada_si256(accGradH + 5 * accPitch + x), gradh);
            gradh = _mm256_add_epi16(loada_si256(accGradH + 6 * accPitch + x), gradh);
            gradv = _mm256_add_epi16(loada_si256(accGradV + 1 * accPitch + x), loada_si256(accGradV + 0 * accPitch + x));
            gradv = _mm256_add_epi16(loada_si256(accGradV + 2 * accPitch + x), gradv);
            gradv = _mm256_add_epi16(loada_si256(accGradV + 3 * accPitch + x), gradv);
            gradv = _mm256_add_epi16(loada_si256(accGradV + 4 * accPitch + x), gradv);
            gradv = _mm256_add_epi16(loada_si256(accGradV + 5 * accPitch + x), gradv);
            gradv = _mm256_add_epi16(loada_si256(accGradV + 6 * accPitch + x), gradv);
            for (int32_t y = 0; y < MIN(TrueTileHeight-3-tileHeight, HASH_WORK_LINES + 8); y += 2) {
                dc = _mm256_sub_epi16(dc, dc0);
                dc = _mm256_add_epi16(loada_si256(acc + (y + 3) * accPitch + x), dc);
                dc0 = loada_si256(acc + (y + 0) * accPitch + x);
                storea_si256(acc + (y + 0) * accPitch + x, dc);
                dc = _mm256_sub_epi16(dc, dc0);
                dc = _mm256_add_epi16(loada_si256(acc + (y + 4) * accPitch + x), dc);
                dc0 = loada_si256(acc + (y + 1) * accPitch + x);
                storea_si256(acc + (y + 1) * accPitch + x, dc);

                gradh = _mm256_sub_epi16(gradh, gradh0);
                gradh = _mm256_add_epi16(loada_si256(accGradH + (y + 7) * accPitch + x), gradh);
                gradh0 = loada_si256(accGradH + (y + 0) * accPitch + x);
                storea_si256(accGradH + (y + 0) * accPitch + x, gradh);
                gradh = _mm256_sub_epi16(gradh, gradh0);
                gradh = _mm256_add_epi16(loada_si256(accGradH + (y + 8) * accPitch + x), gradh);
                gradh0 = loada_si256(accGradH + (y + 1) * accPitch + x);
                storea_si256(accGradH + (y + 1) * accPitch + x, gradh);

                gradv = _mm256_sub_epi16(gradv, gradv0);
                gradv = _mm256_add_epi16(loada_si256(accGradV + (y + 7) * accPitch + x), gradv);
                gradv0 = loada_si256(accGradV + (y + 0) * accPitch + x);
                storea_si256(accGradV + (y + 0) * accPitch + x, gradv);
                gradv = _mm256_sub_epi16(gradv, gradv0);
                gradv = _mm256_add_epi16(loada_si256(accGradV + (y + 8) * accPitch + x), gradv);
                gradv0 = loada_si256(accGradV + (y + 1) * accPitch + x);
                storea_si256(accGradV + (y + 1) * accPitch + x, gradv);
            }
        }

        // 8x8 hashes
        for (int32_t y = 0; y < MIN(TrueTileHeight - 7 - tileHeight, HASH_WORK_LINES); y++) {
            const __m256i maskAligned = (y & 7) ? _mm256_setzero_si256() : _mm256_setr_epi64x(0xffff, 0, 0xffff, 0);
            for (int32_t x = 0; x < tileWidth - 7; x += 16) {
                uint16_t *hashBuf = hashStore + (y+tileHeight) * accPitch + x;

                __m256i dc0, dc1, dc2, dc3, dc8x8, gradh, gradv, grad, hsh, nonZeroGrad, skipIbc;
                dc0 = loada_si256(acc + (y + 0) * accPitch + x + 0);
                dc1 = loadu_si256(acc + (y + 0) * accPitch + x + 4);
                dc2 = loada_si256(acc + (y + 4) * accPitch + x + 0);
                dc3 = loadu_si256(acc + (y + 4) * accPitch + x + 4);
                dc8x8 = _mm256_add_epi16(_mm256_add_epi16(dc0, dc1), _mm256_add_epi16(dc2, dc3));
                storea_si256(acc + y * accPitch + x + 0, dc8x8);
                gradh = loada_si256(accGradH + y * accPitch + x);
                gradv = loada_si256(accGradV + y * accPitch + x);
                grad = _mm256_adds_epu16(gradh, gradv);
                nonZeroGrad = _mm256_cmpgt_epi16(gradh, _mm256_setzero_si256());
                nonZeroGrad = _mm256_and_si256(nonZeroGrad, _mm256_cmpgt_epi16(gradv, _mm256_setzero_si256()));

                storea_si256(accGradH + y * accPitch + x, _mm256_add_epi16(
                    _mm256_add_epi16(gradh,
                        loadu_si256(accGradH + (y + 0) * accPitch + x + 8)),
                    _mm256_add_epi16(loada_si256(accGradH + (y + 8) * accPitch + x + 0),
                        loadu_si256(accGradH + (y + 8) * accPitch + x + 8))));
                storea_si256(accGradV + y * accPitch + x, _mm256_add_epi16(
                    _mm256_add_epi16(gradv,
                        loadu_si256(accGradV + (y + 0) * accPitch + x + 8)),
                    _mm256_add_epi16(loada_si256(accGradV + (y + 8) * accPitch + x + 0),
                        loadu_si256(accGradV + (y + 8) * accPitch + x + 8))));

                grad = _mm256_slli_epi16(grad, 1);
                grad = _mm256_adds_epu16(grad, grad);
                grad = _mm256_srli_epi16(grad, (8) + 4);
                dc0 = _mm256_srli_epi16(dc0, (6 - 2) + 5);
                dc1 = _mm256_srli_epi16(dc1, (6 - 2) + 5);
                dc2 = _mm256_srli_epi16(dc2, (6 - 2) + 5);
                dc3 = _mm256_srli_epi16(dc3, (6 - 2) + 5);

                skipIbc = _mm256_cmpeq_epi16(grad, _mm256_setzero_si256());
                skipIbc = _mm256_and_si256(skipIbc, _mm256_cmpeq_epi16(dc0, dc1));
                skipIbc = _mm256_and_si256(skipIbc, _mm256_cmpeq_epi16(dc0, dc2));
                skipIbc = _mm256_and_si256(skipIbc, _mm256_cmpeq_epi16(dc0, dc3));

                grad = _mm256_slli_epi16(grad, 12);
                dc0 = _mm256_slli_epi16(dc0, 9);
                dc1 = _mm256_slli_epi16(dc1, 6);
                dc2 = _mm256_slli_epi16(dc2, 3);
                hsh = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(_mm256_or_si256(grad, dc0), dc1), dc2), dc3);

                nonZeroGrad = _mm256_or_si256(nonZeroGrad, maskAligned);
                nonZeroGrad = _mm256_andnot_si256(skipIbc, nonZeroGrad);
                hsh = _mm256_and_si256(hsh, nonZeroGrad);

                storea_si256(hashBuf, hsh);

                const int32_t numHashes = std::min(16, tileWidth - 7 - x);
                if ((y & 7) == 0) {
                    const int32_t alignedBlockIdx = ((y + startY + tileHeight) >> 3) * hashIndexes8Pitch + ((x + startX) >> 3);
                    hashIndexes8[alignedBlockIdx].hash = hashBuf[0];

                    if (numHashes >= 8) {
                        hashIndexes8[alignedBlockIdx + 1].hash = hashBuf[8];

                    }
                }
            }
        }
    }



    assert(tileWidth % 8 == 0);
    uint16_t *ph = hashStore;
    for (int32_t y = 0; y < TrueTileHeight - 7; y++, ph += accPitch) {
        int32_t x = 0;
        for (; x < tileWidth - 8; x += 8) {
            if (ph[x + 0]) hashTable8.Count(ph[x + 0]);
            if (ph[x + 1]) hashTable8.Count(ph[x + 1]);
            if (ph[x + 2]) hashTable8.Count(ph[x + 2]);
            if (ph[x + 3]) hashTable8.Count(ph[x + 3]);
            if (ph[x + 4]) hashTable8.Count(ph[x + 4]);
            if (ph[x + 5]) hashTable8.Count(ph[x + 5]);
            if (ph[x + 6]) hashTable8.Count(ph[x + 6]);
            if (ph[x + 7]) hashTable8.Count(ph[x + 7]);
        }
        if (ph[x]) hashTable8.Count(ph[x]);
    }

    hashTable8.Alloc();

}
#endif
