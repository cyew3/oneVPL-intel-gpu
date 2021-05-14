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

#pragma once

#include "cstdint"
#include "vector"
#include "mutex"

namespace AV1Enc {
    struct BlockHash {
        union Packed2Bytes {
            struct {
                uint8_t x;
                uint8_t y;
            };
            uint16_t asInt16;
        } srcSb;
        uint16_t y;
        uint16_t x;
#if !ENABLE_HASH_MEM_REDUCTION
        uint16_t sum;
#endif
    };

    // hash and sum for blocks at aligned positions (8-pix for 8x8 blocks, 16-pix for 16x16, etc)
    struct AlignedBlockHash {
        uint16_t hash;
#if !ENABLE_HASH_MEM_REDUCTION
        uint16_t sum;
#endif
    };

    struct HashTable {
        static const int32_t NUM_BUCKETS = 1 << 16;

        HashTable();
        HashTable(HashTable &&);
        ~HashTable();
        void Clear();
        void Count(uint16_t hashIdx);
        void Alloc();
        void Free();
#if !ENABLE_HASH_MEM_REDUCTION
        void AddBlock(uint16_t hashIdx, uint16_t x, uint16_t y, uint16_t sum, BlockHash::Packed2Bytes srcSb);
#else
        void AddBlock(uint16_t hashIdx, uint16_t x, uint16_t y, BlockHash::Packed2Bytes srcSb);
#endif
        const BlockHash *GetBucketBegin(uint16_t hashIdx) const;
        const BlockHash *GetBucketEnd(uint16_t hashIdx) const;
        const int32_t GetBucketSize(uint16_t hashIdx) const;
        int32_t GetNumBlocks() const;

        HashTable(const HashTable &) = delete;
        void operator=(const HashTable &) = delete;

    //private:
        BlockHash *m_blocks;
        int32_t    m_numBlocksAllocated;
        int32_t    m_numBlocks;
        int32_t    m_bucketRange[NUM_BUCKETS][2];
    };

    // forward declarations
    struct TileBorders;
    struct FrameData;
#if ENABLE_HASH_MEM_REDUCTION
    void AllocTmpBuf(const TileBorders &tb, uint16_t **tmpBufForHash, int32_t *tmpBufForHashSize, uint16_t **workBufForHash);
#else
    void AllocTmpBuf(const TileBorders &tb, uint16_t **tmpBufForHash, int32_t *tmpBufForHashSize);
#endif

    void BuildHashTable_avx2(const FrameData &frameData, const TileBorders &tileBorderMi, uint16_t *tmpBufForHash,
                             HashTable &hashTable8, std::vector<AlignedBlockHash> &hashIndexes8
#if !ENABLE_ONLY_HASH8
                             , HashTable &hashTable16, std::vector<AlignedBlockHash> &hashIndexes16
#elif ENABLE_HASH_MEM_REDUCTION
                            , uint16_t *workBufForHash
#endif
    );
#if ENABLE_HASH_MEM_REDUCTION
    void BuildHashTable_avx2_lowmem(const FrameData &frameData, const TileBorders &tileBorderMi, uint16_t *tmpBufForHash,
        HashTable &hashTable8, std::vector<AlignedBlockHash> &hashIndexes8, uint16_t *workBufForHash);
#endif
};

