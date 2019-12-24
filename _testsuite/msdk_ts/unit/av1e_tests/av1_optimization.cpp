//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 - 2019 Intel Corporation. All Rights Reserved.
//

#include "gtest/gtest.h"

#include "memory"
#include "algorithm"
#include "type_traits"

#define MFX_TARGET_OPTIMIZATION_AUTO
#define MFX_ENABLE_AV1_VIDEO_ENCODE
#include "mfx_av1_dispatcher_proto.h"
#include "mfx_av1_defs.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_quant.h"
#undef min
#undef max

static bool printTicks = getenv("HEVCE_TESTS_PRINT_TICKS")!=NULL;

#define IMPLIES(a, b) (!(a) || (b))

#define GET_MIN_TICKS(ticks, nrepeat, func, args) { \
    ticks = std::numeric_limits<uint64_t>::max();   \
    for (int i = 0; i < nrepeat; i++) {             \
        uint64_t start = _rdtsc();                  \
        func args;                                  \
        uint64_t stop = _rdtsc();                   \
        uint64_t t = stop - start;                  \
        ticks = std::min(ticks, t);                 \
    }                                               \
}

namespace utils {
    enum { AlignCache = 64, AlignAvx2 = 32, AlignSse4 = 16, AlignMax = AlignAvx2 };

    template<typename T> T RoundUpPowerOf2(T val, int alignment) {
        assert((alignment & (alignment - 1)) == 0); // power of 2
        return (val + alignment - 1) & ~(alignment - 1);
    }

    // it seems that gcc doesn't support std::align
    // used implementation of std::align from MS STL
    void *myalign(size_t alignment, size_t size, void *&ptr, size_t &space)
	{
	    size_t offset = alignment == 0 ? 0 : (size_t)((uintptr_t)ptr & (alignment - 1));
	    if (0 < offset)
		    offset = alignment - offset;	// number of bytes to skip
	    if (ptr == 0 || space < offset || space - offset < size)
		    return 0;
	    else
		{	// enough room, update
		    char *ans = (char *)ptr + offset;
		    ptr = (void *)(ans + size);
		    space -= offset + size;
		    return ((void *)ans);
		}
	}

    void *AlignedMalloc(size_t size, size_t align)
    {
        align = std::max(align, sizeof(void *));
        size_t allocsize = size + align + sizeof(void *);
        void *ptr = malloc(allocsize);
        if (ptr == nullptr)
            throw std::bad_alloc();
        size_t space = allocsize - sizeof(void *);
        void *alignedPtr = (char *)ptr + sizeof(void *);
        alignedPtr = myalign(align, size, alignedPtr, space);
        if (alignedPtr == nullptr) {
            free(ptr);
            throw std::bad_alloc();
        }
        *((void **)alignedPtr - 1) = ptr;
        return alignedPtr;
    }

    void AlignedFree(void *ptr) throw()
    {
        if (ptr) {
            ptr = (char *)ptr - sizeof(void *);
            ptr = *(void **)ptr;
            free(ptr);
        }
    }

    template <class T>
    std::unique_ptr<T, decltype(&AlignedFree)> MakeAlignedPtr(size_t size, size_t align)
    {
        T *ptr = static_cast<T *>(utils::AlignedMalloc(size * sizeof(T), align));
        return std::unique_ptr<T, decltype(&utils::AlignedFree)>(ptr, &utils::AlignedFree);
    }

    template <class T, class U>
    void InitConstBlock(T* block, int pitch, int width, int height, U val)
    {
        for (int r = 0; r < height; r++, block += pitch)
            for (int c = 0; c < width; c++)
                block[c] = val;
    }

    void InitRandomDiffBlock(short *block, int pitch, int w, int h, int bd)
    {
        assert(w <= pitch);
        assert(bd <= 15);
        const short mask = (1 << bd) - 1;
        for (int i = 0; i < h; i++)
            for (int j = 0; j < w; j++)
                block[i * pitch + j] = (rand() & mask) - (rand() & mask);
    }

    void InitRandomDiffBuffer(short *buffer, int size, int bd)
    {
        InitRandomDiffBlock(buffer, size, size, 1, bd);
    }

    uint16_t rand_u16()
    {
        return uint16_t((rand() << 1) | (rand() & 1));
    }

    uint32_t rand_u32()
    {
        return (uint32_t(rand()) << 17) | (uint32_t(rand()) << 2) | uint32_t(rand() & 0x3);
    }

    uint64_t rand_u64()
    {
        return (uint64_t(rand()) << 49) | (uint64_t(rand()) << 34) | (uint64_t(rand()) << 19) | (uint64_t(rand()) << 4) | uint64_t(rand() & 15);
    }

    template <typename T>
    void InitRandomBlock(T *block, int pitch, int w, int h, int bd)
    {
        assert(w <= pitch);
        assert(bd <= 64);
        assert(IMPLIES(bd > 8, sizeof(T) >= 2));
        assert(IMPLIES(bd > 16, sizeof(T) >= 4));
        assert(IMPLIES(bd > 32, sizeof(T) >= 8));

        if (bd < 16) {
            const short mask = (1 << bd) - 1;
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>((rand() & mask));
        } else if (bd == 16) {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand_u16());
        } else if (bd < 33) {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand_u32());
        } else {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand_u64());
        }
    }

    template <typename T, typename U>
    void InitRandomBlock(T *block, int pitch, int w, int h, U minVal, U maxVal)
    {
        assert(w <= pitch);
        assert(maxVal >= minVal);
        assert(maxVal <= std::numeric_limits<T>::max());
        assert(minVal >= std::numeric_limits<T>::min());

        std::make_unsigned<U>::type range(maxVal - minVal);
        if (range <= RAND_MAX) {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand() % (range + 1) + minVal);
        } else if (range < std::numeric_limits<uint16_t>::max()) {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand_u16() % (range + 1) + minVal);
        } else if (range == std::numeric_limits<uint16_t>::max()) {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand_u16());
        } else if (range < std::numeric_limits<uint32_t>::max()) {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand_u32() % (range + 1) + minVal);
        } else if (range == std::numeric_limits<uint32_t>::max()) {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand_u32());
        } else if (range < std::numeric_limits<uint64_t>::max()) {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand_u64() % (range + 1) + minVal);
        } else {
            for (int i = 0; i < h; i++)
                for (int j = 0; j < w; j++)
                    block[i * pitch + j] = static_cast<T>(rand_u64());
        }
    }

    template <typename T>
    void InitRandomBuffer(T *buffer, int size, int bd)
    {
        InitRandomBlock(buffer, size, size, 1, bd);
    }

    template <typename T, typename U>
    void InitRandomBuffer(T *buffer, int size, U minVal, U maxVal)
    {
        InitRandomBlock(buffer, size, size, 1, minVal, maxVal);
    }

    template <typename T>
    bool BlocksAreEqual(const T *b0, int pitch0, const T *b1, int pitch1, int w, int h)
    {
        for (int i = 0; i < h; i++)
            for (int j = 0; j < w; j++)
                if (b0[i * pitch0 + j] != b1[i * pitch1 + j])
                    return false;
        return true;
    }

    template <typename T>
    bool BuffersAreEqual(const T *b0, const T *b1, int len)
    {
        for (int i = 0; i < len; i++)
            if (b0[i] != b1[i])
                return false;
        return true;
    }

    template <typename Func, typename... Args> mfxI64 GetMinTicks(int nrepeat, Func func, Args&&... args)
    {
        mfxI64 fastest = std::numeric_limits<mfxI64>::max();
        for (int i = 0; i < nrepeat; i++) {
            mfxI64 start = _rdtsc();
            func(args...);
            mfxI64 stop = _rdtsc();
            mfxI64 ticks = stop - start;
            fastest = std::min(fastest, ticks);
        }
        return fastest;
    }
}

TEST(optimization, sad) {
    const int pitch = 1920;
    const int size = 128 * pitch;

    auto src_ptr = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignCache);
    auto refg_ptr = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignCache);
    auto refs_ptr = utils::MakeAlignedPtr<unsigned char>(64 * 64, utils::AlignCache);
    auto src = src_ptr.get();
    auto refg = refg_ptr.get();
    auto refs = refs_ptr.get();

    srand(0x1234);
    utils::InitRandomBlock(src, pitch, 65, 64, 8);
    utils::InitRandomBlock(refg, pitch, 65, 64, 8);
    utils::InitRandomBlock(refs, 64, 64, 64, 8);

    EXPECT_EQ((AV1PP::sad_special_avx2<4,4>  (src, pitch, refs)), (AV1PP::sad_special_px<4,4>  (src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<4,8>  (src, pitch, refs)), (AV1PP::sad_special_px<4,8>  (src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<8,4>  (src, pitch, refs)), (AV1PP::sad_special_px<8,4>  (src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<8,8>  (src, pitch, refs)), (AV1PP::sad_special_px<8,8>  (src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<8,16> (src, pitch, refs)), (AV1PP::sad_special_px<8,16> (src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<16,8> (src, pitch, refs)), (AV1PP::sad_special_px<16,8> (src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<16,16>(src, pitch, refs)), (AV1PP::sad_special_px<16,16>(src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<16,32>(src, pitch, refs)), (AV1PP::sad_special_px<16,32>(src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<32,16>(src, pitch, refs)), (AV1PP::sad_special_px<32,16>(src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<32,32>(src, pitch, refs)), (AV1PP::sad_special_px<32,32>(src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<32,64>(src, pitch, refs)), (AV1PP::sad_special_px<32,64>(src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<64,32>(src, pitch, refs)), (AV1PP::sad_special_px<64,32>(src, pitch, refs)));
    EXPECT_EQ((AV1PP::sad_special_avx2<64,64>(src, pitch, refs)), (AV1PP::sad_special_px<64,64>(src, pitch, refs)));

    for (int alignment = 0; alignment <= 1; alignment++) {
        EXPECT_EQ((AV1PP::sad_general_avx2<4,4>  (refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<4,4>  (refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<4,8>  (refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<4,8>  (refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<8,4>  (refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<8,4>  (refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<8,8>  (refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<8,8>  (refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<8,16> (refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<8,16> (refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<16,8> (refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<16,8> (refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<16,16>(refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<16,16>(refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<16,32>(refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<16,32>(refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<32,16>(refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<32,16>(refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<32,32>(refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<32,32>(refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<32,64>(refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<32,64>(refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<64,32>(refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<64,32>(refg + alignment, pitch, src, pitch)));
        EXPECT_EQ((AV1PP::sad_general_avx2<64,64>(refg + alignment, pitch, src, pitch)), (AV1PP::sad_general_px<64,64>(refg + alignment, pitch, src, pitch)));
    }

    if (printTicks) {
        Ipp64u ticksAvx2, ticksPx;
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_special_avx2<4,4>, src, pitch, refs);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_special_px<4,4>,   src, pitch, refs);
        printf(" 4x4  special ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_special_avx2<8,8>, src, pitch, refs);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_special_px<8,8>,   src, pitch, refs);
        printf(" 8x8  special ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_special_avx2<16,16>, src, pitch, refs);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_special_px<16,16>,   src, pitch, refs);
        printf("16x16 special ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_special_avx2<32,32>, src, pitch, refs);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_special_px<32,32>,   src, pitch, refs);
        printf("32x32 special ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_special_avx2<64,64>, src, pitch, refs);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_special_px<64,64>,   src, pitch, refs);
        printf("64x64 special ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);

        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_general_avx2<4,4>, src, pitch, refg, pitch);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_general_px<4,4>,   src, pitch, refg, pitch);
        printf(" 4x4  general ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_general_avx2<8,8>, src, pitch, refg, pitch);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_general_px<8,8>,   src, pitch, refg, pitch);
        printf(" 8x8  general ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_general_avx2<16,16>, src, pitch, refg, pitch);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_general_px<16,16>,   src, pitch, refg, pitch);
        printf("16x16 general ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_general_avx2<32,32>, src, pitch, refg, pitch);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_general_px<32,32>,   src, pitch, refg, pitch);
        printf("32x32 general ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_general_avx2<64,64>, src, pitch, refg, pitch);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_general_px<64,64>,   src, pitch, refg, pitch);
        printf("64x64 general ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
    }
}


TEST(optimization, sad_store8x8) {
    const int size = 64;
    const int square_size = size * size;
    const int size_sad = 8;
    const int square_size_sad = size_sad * size_sad;

    auto ptr1 = utils::MakeAlignedPtr<unsigned char>(square_size, utils::AlignCache);
    auto ptr2 = utils::MakeAlignedPtr<unsigned char>(square_size, utils::AlignCache);
    auto sad_tst_ptr = utils::MakeAlignedPtr<int>(square_size_sad, utils::AlignCache);
    auto sad_ref_ptr = utils::MakeAlignedPtr<int>(square_size_sad, utils::AlignCache);
    auto p1 = ptr1.get();
    auto p2 = ptr2.get();
    auto sads_tst = sad_tst_ptr.get();
    auto sads_ref = sad_ref_ptr.get();

    srand(0x1234);
    utils::InitRandomBlock(p1, size, size, size, 8);
    utils::InitRandomBlock(p2, size, size, size, 8);
    utils::InitRandomBlock(sads_tst, size_sad, size_sad, size_sad, 31);
    memcpy(sads_tst, sads_ref, sizeof(*sads_tst) * square_size_sad);

    { SCOPED_TRACE("Testing 64x64");
        ASSERT_EQ(AV1PP::sad_store8x8_px<64>(p1, p2, sads_ref), AV1PP::sad_store8x8_avx2<64>(p1, p2, sads_tst));
        ASSERT_EQ(0, memcmp(sads_ref, sads_tst, sizeof(*sads_tst) * square_size_sad));
    }

    { SCOPED_TRACE("Testing 32x32");
        ASSERT_EQ(AV1PP::sad_store8x8_px<32>(p1, p2, sads_ref), AV1PP::sad_store8x8_avx2<32>(p1, p2, sads_tst));
        ASSERT_EQ(0, memcmp(sads_ref, sads_tst, sizeof(*sads_tst) * square_size_sad));
    }

    { SCOPED_TRACE("Testing 16x16");
        ASSERT_EQ(AV1PP::sad_store8x8_px<16>(p1, p2, sads_ref), AV1PP::sad_store8x8_avx2<16>(p1, p2, sads_tst));
        ASSERT_EQ(0, memcmp(sads_ref, sads_tst, sizeof(*sads_tst) * square_size_sad));
    }

    { SCOPED_TRACE("Testing 8x8");
        ASSERT_EQ(AV1PP::sad_store8x8_px  <8>(p1, p2, sads_ref), AV1PP::sad_store8x8_avx2<8>(p1, p2, sads_tst));
        ASSERT_EQ(0, memcmp(sads_ref, sads_tst, sizeof(*sads_tst) * square_size_sad));
    }

    if (printTicks) {
        Ipp64u ticksAvx2, ticksPx;
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_store8x8_avx2<8>, p1, p2, sads_tst);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sad_store8x8_px  <8>, p1, p2, sads_ref);
        printf(" 8x8  ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_store8x8_avx2<16>, p1, p2, sads_tst);
        ticksPx   = utils::GetMinTicks(300000, AV1PP::sad_store8x8_px  <16>, p1, p2, sads_ref);
        printf("16x16 ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_store8x8_avx2<32>, p1, p2, sads_tst);
        ticksPx   = utils::GetMinTicks(100000, AV1PP::sad_store8x8_px  <32>, p1, p2, sads_ref);
        printf("32x32 ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sad_store8x8_avx2<64>, p1, p2, sads_tst);
        ticksPx   = utils::GetMinTicks(100000, AV1PP::sad_store8x8_px  <64>, p1, p2, sads_ref);
        printf("64x64 ref=%-6lld avx2=%-4lld\n", ticksPx, ticksAvx2);
    }
}

TEST(optimization, sse) {
    Ipp32s pitch1 = 128;
    Ipp32s pitch2 = 128;
    auto src1_ptr = utils::MakeAlignedPtr<unsigned char>(pitch1 * 64, utils::AlignAvx2);
    auto src2_ptr = utils::MakeAlignedPtr<unsigned char>(pitch2 * 64, utils::AlignAvx2);
    auto src1 = src1_ptr.get();
    auto src2 = src2_ptr.get();

    srand(0x1234);
    utils::InitRandomBlock(src1, pitch1, 64, 64, 8);
    utils::InitRandomBlock(src2, pitch2, 64, 64, 8);

    EXPECT_EQ((AV1PP::sse_avx2< 4, 4>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px< 4, 4>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2< 4, 8>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px< 4, 8>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2< 8, 4>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px< 8, 4>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2< 8, 8>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px< 8, 8>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2< 8,16>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px< 8,16>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<16, 4>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<16, 4>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<16, 8>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<16, 8>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<16,16>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<16,16>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<16,32>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<16,32>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<32, 8>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<32, 8>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<32,16>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<32,16>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<32,32>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<32,32>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<32,64>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<32,64>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<64,16>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<64,16>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<64,32>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<64,32>(src1, pitch1, src2, pitch2)));
    EXPECT_EQ((AV1PP::sse_avx2<64,64>(src1, pitch1, src2, pitch2)), (AV1PP::sse_px<64,64>(src1, pitch1, src2, pitch2)));

    if (printTicks) {
        Ipp64u ticksAvx2, ticksPx;
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sse_avx2<4,4>, src1, pitch1, src2, pitch2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sse_px<4,4, unsigned char>,   src1, pitch1, src2, pitch2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 4, 4, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sse_avx2<8,8>, src1, pitch1, src2, pitch2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sse_px<8,8, unsigned char>,   src1, pitch1, src2, pitch2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 8, 8, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sse_avx2<16,16>, src1, pitch1, src2, pitch2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sse_px<16,16, unsigned char>,   src1, pitch1, src2, pitch2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 16, 16, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(100000, AV1PP::sse_avx2<32,32>, src1, pitch1, src2, pitch2);
        ticksPx   = utils::GetMinTicks(100000, AV1PP::sse_px<32,32, unsigned char>,   src1, pitch1, src2, pitch2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 32, 32, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(100000, AV1PP::sse_avx2<64,64>, src1, pitch1, src2, pitch2);
        ticksPx   = utils::GetMinTicks(100000, AV1PP::sse_px<64,64, unsigned char>,   src1, pitch1, src2, pitch2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 64, 64, ticksPx, ticksAvx2);
    }
}


TEST(optimization, sse_p64_pw) {
    Ipp32s pitch1 = 64;
    Ipp32s pitch2 = 64;
    auto src1_ptr = utils::MakeAlignedPtr<unsigned char>(pitch1 * 64, utils::AlignAvx2);
    auto src2_ptr = utils::MakeAlignedPtr<unsigned char>(pitch2 * 64, utils::AlignAvx2);
    auto src1 = src1_ptr.get();
    auto src2 = src2_ptr.get();

    srand(0x1234);
    utils::InitRandomBlock(src1, pitch1, 64, 64, 8);
    utils::InitRandomBlock(src2, pitch2, 64, 64, 8);

    EXPECT_EQ((AV1PP::sse_p64_pw_avx2< 4, 4>(src1, src2)), (AV1PP::sse_p64_pw_px< 4, 4>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2< 4, 8>(src1, src2)), (AV1PP::sse_p64_pw_px< 4, 8>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2< 8, 4>(src1, src2)), (AV1PP::sse_p64_pw_px< 8, 4>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2< 8, 8>(src1, src2)), (AV1PP::sse_p64_pw_px< 8, 8>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2< 8,16>(src1, src2)), (AV1PP::sse_p64_pw_px< 8,16>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<16, 4>(src1, src2)), (AV1PP::sse_p64_pw_px<16, 4>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<16, 8>(src1, src2)), (AV1PP::sse_p64_pw_px<16, 8>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<16,16>(src1, src2)), (AV1PP::sse_p64_pw_px<16,16>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<16,32>(src1, src2)), (AV1PP::sse_p64_pw_px<16,32>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<32, 8>(src1, src2)), (AV1PP::sse_p64_pw_px<32, 8>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<32,16>(src1, src2)), (AV1PP::sse_p64_pw_px<32,16>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<32,32>(src1, src2)), (AV1PP::sse_p64_pw_px<32,32>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<32,64>(src1, src2)), (AV1PP::sse_p64_pw_px<32,64>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<64,16>(src1, src2)), (AV1PP::sse_p64_pw_px<64,16>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<64,32>(src1, src2)), (AV1PP::sse_p64_pw_px<64,32>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_pw_avx2<64,64>(src1, src2)), (AV1PP::sse_p64_pw_px<64,64>(src1, src2)));

    if (printTicks) {
        Ipp64u ticksAvx2, ticksPx;
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sse_p64_pw_avx2<4,4>, src1, src2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sse_p64_pw_px<4,4>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 4, 4, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sse_p64_pw_avx2<8,8>, src1, src2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sse_p64_pw_px<8,8>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 8, 8, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sse_p64_pw_avx2<16,16>, src1, src2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sse_p64_pw_px<16,16>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 16, 16, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(100000, AV1PP::sse_p64_pw_avx2<32,32>, src1, src2);
        ticksPx   = utils::GetMinTicks(100000, AV1PP::sse_p64_pw_px<32,32>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 32, 32, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(100000, AV1PP::sse_p64_pw_avx2<64,64>, src1, src2);
        ticksPx   = utils::GetMinTicks(100000, AV1PP::sse_p64_pw_px<64,64>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 64, 64, ticksPx, ticksAvx2);
    }
}


TEST(optimization, sse_p64_p64) {
    Ipp32s pitch1 = 64;
    Ipp32s pitch2 = 64;
    auto src1_ptr = utils::MakeAlignedPtr<unsigned char>(pitch1 * 64, utils::AlignAvx2);
    auto src2_ptr = utils::MakeAlignedPtr<unsigned char>(pitch2 * 64, utils::AlignAvx2);
    auto src1 = src1_ptr.get();
    auto src2 = src2_ptr.get();

    srand(0x1234);
    utils::InitRandomBlock(src1, pitch1, 64, 64, 8);
    utils::InitRandomBlock(src2, pitch2, 64, 64, 8);

    EXPECT_EQ((AV1PP::sse_p64_p64_avx2< 4, 4>(src1, src2)), (AV1PP::sse_p64_p64_px< 4, 4>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2< 4, 8>(src1, src2)), (AV1PP::sse_p64_p64_px< 4, 8>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2< 8, 4>(src1, src2)), (AV1PP::sse_p64_p64_px< 8, 4>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2< 8, 8>(src1, src2)), (AV1PP::sse_p64_p64_px< 8, 8>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2< 8,16>(src1, src2)), (AV1PP::sse_p64_p64_px< 8,16>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<16, 4>(src1, src2)), (AV1PP::sse_p64_p64_px<16, 4>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<16, 8>(src1, src2)), (AV1PP::sse_p64_p64_px<16, 8>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<16,16>(src1, src2)), (AV1PP::sse_p64_p64_px<16,16>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<16,32>(src1, src2)), (AV1PP::sse_p64_p64_px<16,32>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<32, 8>(src1, src2)), (AV1PP::sse_p64_p64_px<32, 8>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<32,16>(src1, src2)), (AV1PP::sse_p64_p64_px<32,16>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<32,32>(src1, src2)), (AV1PP::sse_p64_p64_px<32,32>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<32,64>(src1, src2)), (AV1PP::sse_p64_p64_px<32,64>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<64,16>(src1, src2)), (AV1PP::sse_p64_p64_px<64,16>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<64,32>(src1, src2)), (AV1PP::sse_p64_p64_px<64,32>(src1, src2)));
    EXPECT_EQ((AV1PP::sse_p64_p64_avx2<64,64>(src1, src2)), (AV1PP::sse_p64_p64_px<64,64>(src1, src2)));

    if (printTicks) {
        Ipp64u ticksAvx2, ticksPx;
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sse_p64_p64_avx2<4,4>, src1, src2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sse_p64_p64_px<4,4,unsigned char>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 4, 4, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sse_p64_p64_avx2<8,8>, src1, src2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sse_p64_p64_px<8,8, unsigned char>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 8, 8, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::sse_p64_p64_avx2<16,16>, src1, src2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::sse_p64_p64_px<16,16, unsigned char>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 16, 16, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(100000, AV1PP::sse_p64_p64_avx2<32,32>, src1, src2);
        ticksPx   = utils::GetMinTicks(100000, AV1PP::sse_p64_p64_px<32,32, unsigned char>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 32, 32, ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(100000, AV1PP::sse_p64_p64_avx2<64,64>, src1, src2);
        ticksPx   = utils::GetMinTicks(100000, AV1PP::sse_p64_p64_px<64,64, unsigned char>,   src1, src2);
        printf("%2dx%-2d  8bit ref=%-4lld avx2=%-4lld\n", 64, 64, ticksPx, ticksAvx2);
    }
}


namespace diff_nxn {
    template<int n> void test(Ipp8u *src, Ipp32s pitchSrc, Ipp8u *pred, Ipp32s pitchPred, Ipp16s *dst_ref, Ipp16s *dst_tst, Ipp32s pitchDst) {
        std::ostringstream buf;
        buf << "Testing " << n << "x" << n;
        SCOPED_TRACE(buf.str().c_str());
        const Ipp32s maxSize = 64;
        memset(dst_ref, 0, sizeof(short) * maxSize * maxSize);
        memset(dst_tst, 0, sizeof(short) * maxSize * maxSize);
        utils::InitRandomBlock(src, pitchSrc, pitchSrc, maxSize, 8);
        utils::InitRandomBlock(pred, pitchPred, pitchPred, maxSize, 8);
        AV1PP::diff_nxn_px<n>  (src, pitchSrc, pred, pitchPred, dst_ref, maxSize);
        AV1PP::diff_nxn_avx2<n>(src, pitchSrc, pred, pitchPred, dst_tst, maxSize);
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(short) * maxSize * maxSize));
    }
};

TEST(optimization, diff_nxn) {
    Ipp32s maxSize = 64;
    Ipp32s pitchSrc  = 128;
    Ipp32s pitchPred = 256;

    auto src_ptr   = utils::MakeAlignedPtr<unsigned char>(pitchSrc  * maxSize, utils::AlignAvx2);
    auto pred_ptr  = utils::MakeAlignedPtr<unsigned char>(pitchPred * maxSize, utils::AlignAvx2);
    auto diff_ref_ptr = utils::MakeAlignedPtr<short>(maxSize * maxSize, utils::AlignAvx2);
    auto diff_tst_ptr = utils::MakeAlignedPtr<short>(maxSize * maxSize, utils::AlignAvx2);
    auto src = src_ptr.get();
    auto pred = pred_ptr.get();
    auto diff_ref = diff_ref_ptr.get();
    auto diff_tst = diff_tst_ptr.get();

    srand(0x1234);

    diff_nxn::test<4>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize);
    diff_nxn::test<8>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize);
    diff_nxn::test<16>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize);
    diff_nxn::test<32>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize);
    diff_nxn::test<64>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize);

    if (printTicks) {
        Ipp64u ticksTst, ticksRef;
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxn_avx2<4>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxn_px<4>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize);
        printf(" 4x4  8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxn_avx2<8>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxn_px<8>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize);
        printf(" 8x8  8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxn_avx2<16>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxn_px<16>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize);
        printf("16x16 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(100000, AV1PP::diff_nxn_avx2<32>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize);
        ticksRef = utils::GetMinTicks(100000, AV1PP::diff_nxn_px<32>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize);
        printf("32x32 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(100000, AV1PP::diff_nxn_avx2<64>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize);
        ticksRef = utils::GetMinTicks(100000, AV1PP::diff_nxn_px<64>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize);
        printf("64x64 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
    }
}


namespace diff_nxn_p64_p64_pw {
    template<int n> void test(Ipp8u *src, Ipp8u *pred, Ipp16s *dst_ref, Ipp16s *dst_tst) {
        std::ostringstream buf;
        buf << "Testing " << n << "x" << n;
        SCOPED_TRACE(buf.str().c_str());
        const Ipp32s maxSize = 64;
        memset(dst_ref, 0, sizeof(short) * maxSize * maxSize);
        memset(dst_tst, 0, sizeof(short) * maxSize * maxSize);
        utils::InitRandomBlock(src, maxSize, maxSize, maxSize, 8);
        utils::InitRandomBlock(pred, maxSize, maxSize, maxSize, 8);
        AV1PP::diff_nxn_p64_p64_pw_px<n>  (src, pred, dst_ref);
        AV1PP::diff_nxn_p64_p64_pw_avx2<n>(src, pred, dst_tst);
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * maxSize * maxSize));
    }
};

TEST(optimization, diff_nxn_p64_p64_pw) {
    Ipp32s maxSize = 64;
    Ipp32s pitchSrc  = 64;
    Ipp32s pitchPred = 64;
    auto src_ptr   = utils::MakeAlignedPtr<unsigned char>(pitchSrc  * maxSize, utils::AlignAvx2);
    auto pred_ptr  = utils::MakeAlignedPtr<unsigned char>(pitchPred * maxSize, utils::AlignAvx2);
    auto diff_ref_ptr = utils::MakeAlignedPtr<short>(maxSize * maxSize, utils::AlignAvx2);
    auto diff_tst_ptr = utils::MakeAlignedPtr<short>(maxSize * maxSize, utils::AlignAvx2);
    auto src = src_ptr.get();
    auto pred = pred_ptr.get();
    auto diff_ref = diff_ref_ptr.get();
    auto diff_tst = diff_tst_ptr.get();

    srand(0x1234);

    diff_nxn_p64_p64_pw::test<4>(src, pred, diff_ref, diff_tst);
    diff_nxn_p64_p64_pw::test<8>(src, pred, diff_ref, diff_tst);
    diff_nxn_p64_p64_pw::test<16>(src, pred, diff_ref, diff_tst);
    diff_nxn_p64_p64_pw::test<32>(src, pred, diff_ref, diff_tst);
    diff_nxn_p64_p64_pw::test<64>(src, pred, diff_ref, diff_tst);

    if (printTicks) {
        Ipp64u ticksTst, ticksRef;
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxn_p64_p64_pw_avx2<4>, src, pred, diff_tst);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxn_p64_p64_pw_px<4, unsigned char>,   src, pred, diff_ref);
        printf(" 4x4  8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxn_p64_p64_pw_avx2<8>, src, pred, diff_tst);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxn_p64_p64_pw_px<8, unsigned char>,   src, pred, diff_ref);
        printf(" 8x8  8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxn_p64_p64_pw_avx2<16>, src, pred, diff_tst);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxn_p64_p64_pw_px<16, unsigned char>,   src, pred, diff_ref);
        printf("16x16 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(100000, AV1PP::diff_nxn_p64_p64_pw_avx2<32>, src, pred, diff_tst);
        ticksRef = utils::GetMinTicks(100000, AV1PP::diff_nxn_p64_p64_pw_px<32, unsigned char>,   src, pred, diff_ref);
        printf("32x32 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(100000, AV1PP::diff_nxn_p64_p64_pw_avx2<64>, src, pred, diff_tst);
        ticksRef = utils::GetMinTicks(100000, AV1PP::diff_nxn_p64_p64_pw_px<64, unsigned char>,   src, pred, diff_ref);
        printf("64x64 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
    }
}


namespace diff_nxm {
    template<int n> void test(Ipp8u *src, Ipp32s pitchSrc, Ipp8u *pred, Ipp32s pitchPred, Ipp16s *dst_ref, Ipp16s *dst_tst, Ipp32s pitchDst, Ipp32s m) {
        std::ostringstream buf;
        buf << "Testing " << n << "x" << m;
        SCOPED_TRACE(buf.str().c_str());
        const Ipp32s maxSize = 64;
        memset(dst_ref, 0, sizeof(short) * maxSize * maxSize);
        memset(dst_tst, 0, sizeof(short) * maxSize * maxSize);
        utils::InitRandomBlock(src, pitchSrc, pitchSrc, maxSize, 8);
        utils::InitRandomBlock(pred, pitchPred, pitchPred, maxSize, 8);
        AV1PP::diff_nxm_px<n>  (src, pitchSrc, pred, pitchPred, dst_ref, maxSize, m);
        AV1PP::diff_nxm_avx2<n>(src, pitchSrc, pred, pitchPred, dst_tst, maxSize, m);
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(short) * maxSize * maxSize));
    }
};

TEST(optimization, diff_nxm) {
    Ipp32s maxSize = 64;
    Ipp32s pitchSrc  = 128;
    Ipp32s pitchPred = 256;

    auto src_ptr   = utils::MakeAlignedPtr<unsigned char>(pitchSrc  * maxSize, utils::AlignAvx2);
    auto pred_ptr  = utils::MakeAlignedPtr<unsigned char>(pitchPred * maxSize, utils::AlignAvx2);
    auto diff_ref_ptr = utils::MakeAlignedPtr<short>(maxSize * maxSize, utils::AlignAvx2);
    auto diff_tst_ptr = utils::MakeAlignedPtr<short>(maxSize * maxSize, utils::AlignAvx2);
    auto src = src_ptr.get();
    auto pred = pred_ptr.get();
    auto diff_ref = diff_ref_ptr.get();
    auto diff_tst = diff_tst_ptr.get();

    srand(0x1234);

    diff_nxm::test<4>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 4);
    diff_nxm::test<4>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 8);
    diff_nxm::test<8>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 4);
    diff_nxm::test<8>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 8);
    diff_nxm::test<8>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 16);
    diff_nxm::test<16>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 8);
    diff_nxm::test<16>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 16);
    diff_nxm::test<16>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 32);
    diff_nxm::test<32>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 16);
    diff_nxm::test<32>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 32);
    diff_nxm::test<32>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 64);
    diff_nxm::test<64>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 32);
    diff_nxm::test<64>(src, pitchSrc, pred, pitchPred, diff_ref, diff_tst, maxSize, 64);

    if (printTicks) {
        Ipp64u ticksTst, ticksRef;
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxm_avx2<4>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 4);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxm_px<4>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 4);
        printf(" 4x4  8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxm_avx2<8>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 8);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxm_px<8>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 8);
        printf(" 8x8  8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxm_avx2<16>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 16);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxm_px<16>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 16);
        printf("16x16 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(100000, AV1PP::diff_nxm_avx2<32>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 32);
        ticksRef = utils::GetMinTicks(100000, AV1PP::diff_nxm_px<32>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 32);
        printf("32x32 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(100000, AV1PP::diff_nxm_avx2<64>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 64);
        ticksRef = utils::GetMinTicks(100000, AV1PP::diff_nxm_px<64>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 64);
        printf("64x64 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
    }
}


namespace diff_nxm_p64_p64_pw {
    template<int n> void test(Ipp8u *src, Ipp8u *pred, Ipp16s *dst_ref, Ipp16s *dst_tst, Ipp32s m) {
        std::ostringstream buf;
        buf << "Testing " << n << "x" << m;
        SCOPED_TRACE(buf.str().c_str());
        const Ipp32s maxSize = 64;
        memset(dst_ref, 0, sizeof(short) * maxSize * maxSize);
        memset(dst_tst, 0, sizeof(short) * maxSize * maxSize);
        utils::InitRandomBlock(src, maxSize, maxSize, maxSize, 8);
        utils::InitRandomBlock(pred, maxSize, maxSize, maxSize, 8);
        AV1PP::diff_nxm_p64_p64_pw_px<n>  (src, pred, dst_ref, m);
        AV1PP::diff_nxm_p64_p64_pw_avx2<n>(src, pred, dst_tst, m);
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(short) * maxSize * maxSize));
    }
};

TEST(optimization, diff_nxm_p64_p64_pw) {
    Ipp32s maxSize = 64;
    Ipp32s pitchSrc  = 64;
    Ipp32s pitchPred = 64;

    auto src_ptr   = utils::MakeAlignedPtr<unsigned char>(pitchSrc  * maxSize, utils::AlignAvx2);
    auto pred_ptr  = utils::MakeAlignedPtr<unsigned char>(pitchPred * maxSize, utils::AlignAvx2);
    auto diff_ref_ptr = utils::MakeAlignedPtr<short>(maxSize * maxSize, utils::AlignAvx2);
    auto diff_tst_ptr = utils::MakeAlignedPtr<short>(maxSize * maxSize, utils::AlignAvx2);
    auto src = src_ptr.get();
    auto pred = pred_ptr.get();
    auto diff_ref = diff_ref_ptr.get();
    auto diff_tst = diff_tst_ptr.get();

    srand(0x1234);

    diff_nxm_p64_p64_pw::test<4>(src, pred, diff_ref, diff_tst, 4);
    diff_nxm_p64_p64_pw::test<4>(src, pred, diff_ref, diff_tst, 8);
    diff_nxm_p64_p64_pw::test<8>(src, pred, diff_ref, diff_tst, 4);
    diff_nxm_p64_p64_pw::test<8>(src, pred, diff_ref, diff_tst, 8);
    diff_nxm_p64_p64_pw::test<8>(src, pred, diff_ref, diff_tst, 16);
    diff_nxm_p64_p64_pw::test<16>(src, pred, diff_ref, diff_tst, 8);
    diff_nxm_p64_p64_pw::test<16>(src, pred, diff_ref, diff_tst, 16);
    diff_nxm_p64_p64_pw::test<16>(src, pred, diff_ref, diff_tst, 32);
    diff_nxm_p64_p64_pw::test<32>(src, pred, diff_ref, diff_tst, 16);
    diff_nxm_p64_p64_pw::test<32>(src, pred, diff_ref, diff_tst, 32);
    diff_nxm_p64_p64_pw::test<32>(src, pred, diff_ref, diff_tst, 64);
    diff_nxm_p64_p64_pw::test<64>(src, pred, diff_ref, diff_tst, 32);
    diff_nxm_p64_p64_pw::test<64>(src, pred, diff_ref, diff_tst, 64);

    if (printTicks) {
        Ipp64u ticksTst, ticksRef;
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxm_avx2<4>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 4);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxm_px<4>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 4);
        printf(" 4x4  8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxm_avx2<8>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 8);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxm_px<8>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 8);
        printf(" 8x8  8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(1000000, AV1PP::diff_nxm_avx2<16>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 16);
        ticksRef = utils::GetMinTicks(1000000, AV1PP::diff_nxm_px<16>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 16);
        printf("16x16 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(100000, AV1PP::diff_nxm_avx2<32>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 32);
        ticksRef = utils::GetMinTicks(100000, AV1PP::diff_nxm_px<32>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 32);
        printf("32x32 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
        ticksTst = utils::GetMinTicks(100000, AV1PP::diff_nxm_avx2<64>, src, pitchSrc, pred, pitchPred, diff_tst, maxSize, 64);
        ticksRef = utils::GetMinTicks(100000, AV1PP::diff_nxm_px<64>,   src, pitchSrc, pred, pitchPred, diff_ref, maxSize, 64);
        printf("64x64 8bit ref=%-4lld avx2=%-4lld\n", ticksRef, ticksTst);
    }
}


namespace diff_nv12 {
    template<int n> void test(Ipp8u *src, Ipp32s pitchSrc, Ipp8u *pred, Ipp32s pitchPred, Ipp16s *dstu_ref,
                              Ipp16s *dstu_tst, Ipp16s *dstv_ref, Ipp16s *dstv_tst, Ipp32s pitchDst, Ipp32s m) {
        std::ostringstream buf;
        buf << "Testing " << n << "x" << m;
        SCOPED_TRACE(buf.str().c_str());
        const Ipp32s maxSize = 64;
        memset(dstu_ref, 0xff, 2*64*64);
        memset(dstv_ref, 0xff, 2*64*64);
        memset(dstu_tst, 0xff, 2*64*64);
        memset(dstv_tst, 0xff, 2*64*64);
        utils::InitRandomBlock(src, pitchSrc, pitchSrc, maxSize, 8);
        utils::InitRandomBlock(pred, pitchPred, pitchPred, maxSize, 8);
        AV1PP::diff_nv12_px<n>  (src, pitchSrc, pred, pitchPred, dstu_ref, dstv_ref, maxSize, m);
        AV1PP::diff_nv12_avx2<n>(src, pitchSrc, pred, pitchPred, dstu_tst, dstv_tst, maxSize, m);
        for (Ipp32s i = 0; i < 64*64; i++)
            if (dstu_ref[i] != dstu_tst[i])
                printf("dstu_tst[%d] = %d dstu_ref[%d] = %d\n", i, dstu_tst[i], i, dstu_ref[i]);
        ASSERT_EQ(0, memcmp(dstu_tst, dstu_ref, sizeof(short) * 64 * 64));
        ASSERT_EQ(0, memcmp(dstv_tst, dstv_ref, sizeof(short) * 64 * 64));
    }
};

TEST(optimization, diff_nv12) {
    Ipp32s pitchSrc  = 128;
    Ipp32s pitchPred = 256;

    auto src_ptr    = utils::MakeAlignedPtr<unsigned char>(pitchSrc  * 64, utils::AlignAvx2);
    auto pred_ptr   = utils::MakeAlignedPtr<unsigned char>(pitchPred * 64, utils::AlignAvx2);
    auto u_tst_ptr = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto v_tst_ptr = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto u_ref_ptr = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto v_ref_ptr = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto src    = src_ptr.get();
    auto pred   = pred_ptr.get();
    auto u_tst = u_tst_ptr.get();
    auto v_tst = v_tst_ptr.get();
    auto u_ref = u_ref_ptr.get();
    auto v_ref = v_ref_ptr.get();

    srand(0x1234);

    using AV1PP::diff_nv12_px;
    using AV1PP::diff_nv12_avx2;
    typedef void (*fptr_t)(const uint8_t *, int, const uint8_t *, int, int16_t *, int16_t *, int, int);
    const fptr_t fptr_ref[4] = { diff_nv12_px<4>, diff_nv12_px<8>, diff_nv12_px<16>, diff_nv12_px<32> };
    const fptr_t fptr_avx[4] = { diff_nv12_avx2<4>, diff_nv12_avx2<8>, diff_nv12_avx2<16>, diff_nv12_avx2<32> };

    diff_nv12::test<4>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 4);
    diff_nv12::test<4>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 8);
    diff_nv12::test<8>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 4);
    diff_nv12::test<8>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 8);
    diff_nv12::test<8>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 16);
    diff_nv12::test<16>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 8);
    diff_nv12::test<16>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 16);
    diff_nv12::test<16>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 32);
    diff_nv12::test<32>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 16);
    diff_nv12::test<32>(src, pitchSrc, pred, pitchPred, u_ref, v_ref, u_tst, v_tst, 64, 32);


    if (printTicks) {
        utils::InitRandomBlock(src, pitchSrc, 128, 64, 8);
        utils::InitRandomBlock(pred, pitchPred, 128, 64, 8);

        for (int size = 0; size < 4; size++) {
            const int w = 4 << size;
            const int h = 4 << size;
            const uint64_t ticksAvx2 = utils::GetMinTicks(1000000, fptr_ref[size], src, pitchSrc, pred, pitchPred, u_tst, v_tst, 4, 4);
            const uint64_t ticksPx   = utils::GetMinTicks(1000000, fptr_avx[size], src, pitchSrc, pred, pitchPred, u_ref, v_ref, 4, 4);
            printf("%2dx%-2d ref=%-4lld avx2=%-4lld\n", w, h, ticksPx, ticksAvx2);
        }
    }
}


namespace diff_nv12_p64_p64_pw {
    template<int n> void test(Ipp8u *src, Ipp8u *pred,
                              Ipp16s *dstu_ref, Ipp16s *dstu_tst, Ipp16s *dstv_ref, Ipp16s *dstv_tst, Ipp32s m) {
        std::ostringstream buf;
        buf << "Testing " << n << "x" << m;
        SCOPED_TRACE(buf.str().c_str());
        const Ipp32s maxSize = 64;
        memset(dstu_ref, 0xff, 2*64*64);
        memset(dstv_ref, 0xff, 2*64*64);
        memset(dstu_tst, 0xff, 2*64*64);
        memset(dstv_tst, 0xff, 2*64*64);
        utils::InitRandomBlock(src, 64, 64, 64, 8);
        utils::InitRandomBlock(pred, 64, 64, 64, 8);
        AV1PP::diff_nv12_p64_p64_pw_px<n>  (src, pred, dstu_ref, dstv_ref, m);
        AV1PP::diff_nv12_p64_p64_pw_avx2<n>(src, pred, dstu_tst, dstv_tst, m);
        ASSERT_EQ(0, memcmp(dstu_tst, dstu_ref, sizeof(short) * 64 * 64));
        ASSERT_EQ(0, memcmp(dstv_tst, dstv_ref, sizeof(short) * 64 * 64));
    }
};

TEST(optimization, diff_nv12_p64_p64_pw) {
    Ipp32s pitchSrc  = 128;
    Ipp32s pitchPred = 256;

    auto src_ptr    = utils::MakeAlignedPtr<unsigned char>(pitchSrc  * 64, utils::AlignAvx2);
    auto pred_ptr   = utils::MakeAlignedPtr<unsigned char>(pitchPred * 64, utils::AlignAvx2);
    auto u_tst_ptr = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto v_tst_ptr = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto u_ref_ptr = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto v_ref_ptr = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto src    = src_ptr.get();
    auto pred   = pred_ptr.get();
    auto u_tst = u_tst_ptr.get();
    auto v_tst = v_tst_ptr.get();
    auto u_ref = u_ref_ptr.get();
    auto v_ref = v_ref_ptr.get();

    srand(0x1234);

    diff_nv12_p64_p64_pw::test<4>(src, pred, u_ref, v_ref, u_tst, v_tst, 4);
    diff_nv12_p64_p64_pw::test<4>(src, pred, u_ref, v_ref, u_tst, v_tst, 8);
    diff_nv12_p64_p64_pw::test<8>(src, pred, u_ref, v_ref, u_tst, v_tst, 4);
    diff_nv12_p64_p64_pw::test<8>(src, pred, u_ref, v_ref, u_tst, v_tst, 8);
    diff_nv12_p64_p64_pw::test<8>(src, pred, u_ref, v_ref, u_tst, v_tst, 16);
    diff_nv12_p64_p64_pw::test<16>(src, pred, u_ref, v_ref, u_tst, v_tst, 8);
    diff_nv12_p64_p64_pw::test<16>(src, pred, u_ref, v_ref, u_tst, v_tst, 16);
    diff_nv12_p64_p64_pw::test<16>(src, pred, u_ref, v_ref, u_tst, v_tst, 32);
    diff_nv12_p64_p64_pw::test<32>(src, pred, u_ref, v_ref, u_tst, v_tst, 16);
    diff_nv12_p64_p64_pw::test<32>(src, pred, u_ref, v_ref, u_tst, v_tst, 32);


    if (printTicks) {
        utils::InitRandomBlock(src, pitchSrc, 128, 64, 8);
        utils::InitRandomBlock(pred, pitchPred, 128, 64, 8);

        Ipp64u ticksAvx2, ticksPx;
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::diff_nv12_p64_p64_pw_avx2<4>, src, pred, u_tst, v_tst, 4);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::diff_nv12_p64_p64_pw_px<4, unsigned char>,   src, pred, u_ref, v_ref, 4);
        printf(" 4x4  8bit ref=%-4lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::diff_nv12_p64_p64_pw_avx2<8>, src, pred, u_tst, v_tst, 8);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::diff_nv12_p64_p64_pw_px<8, unsigned char>,   src, pred, u_ref, v_ref, 8);
        printf(" 8x8  8bit ref=%-4lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::diff_nv12_p64_p64_pw_avx2<16>, src, pred, u_tst, v_tst, 16);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::diff_nv12_p64_p64_pw_px<16, unsigned char>,   src, pred, u_ref, v_ref, 16);
        printf("16x16 8bit ref=%-4lld avx2=%-4lld\n", ticksPx, ticksAvx2);
        ticksAvx2 = utils::GetMinTicks(100000, AV1PP::diff_nv12_p64_p64_pw_avx2<32>, src, pred, u_tst, v_tst,  32);
        ticksPx   = utils::GetMinTicks(100000, AV1PP::diff_nv12_p64_p64_pw_px<32, unsigned char>,   src, pred, u_ref, v_ref,  32);
        printf("32x32 8bit ref=%-4lld avx2=%-4lld\n", ticksPx, ticksAvx2);
    }
}


TEST(optimization, adds_nv12) {
    Ipp32s pitch = 256;
    Ipp32s maxsize = 64;

    auto residU_ptr   = utils::MakeAlignedPtr<Ipp16s>(pitch * maxsize,   utils::AlignAvx2);
    auto residV_ptr   = utils::MakeAlignedPtr<Ipp16s>(pitch * maxsize,   utils::AlignAvx2);
    auto pred_ptr     = utils::MakeAlignedPtr<Ipp8u> (pitch * maxsize*2, utils::AlignAvx2);
    auto dst_px_ptr   = utils::MakeAlignedPtr<Ipp8u> (pitch * maxsize*2, utils::AlignAvx2);
    auto dst_avx2_ptr = utils::MakeAlignedPtr<Ipp8u> (pitch * maxsize*2, utils::AlignAvx2);
    auto residU = residU_ptr.get();
    auto residV = residV_ptr.get();
    auto pred = pred_ptr.get();
    auto dst_px = dst_px_ptr.get();
    auto dst_avx2 = dst_avx2_ptr.get();

    srand(0x1234);

    for (Ipp32s size = 4; size <= 64; size<<=1) {
        utils::InitRandomDiffBlock(residU, pitch, pitch, maxsize, 8);
        utils::InitRandomDiffBlock(residV, pitch, pitch, maxsize, 8);
        utils::InitRandomBlock(pred,   pitch, pitch, maxsize*2, 8);
        utils::InitRandomBlock(dst_px, pitch, pitch, maxsize*2, 8);
        memcpy(dst_avx2, dst_px, sizeof(Ipp8u) * pitch * maxsize*2);

        std::ostringstream buf;
        buf << "Testing " << size << "x" << size << " 8bit";
        SCOPED_TRACE(buf.str().c_str());
        AV1PP::adds_nv12_px  (dst_px, pitch, pred, pitch, residU, residV, pitch, size);
        AV1PP::adds_nv12_avx2(dst_avx2, pitch, pred, pitch, residU, residV, pitch, size);
        ASSERT_EQ(0, memcmp(dst_avx2, dst_px, sizeof(Ipp8u) * pitch * maxsize*2));
    }

    if (printTicks) {
        for (Ipp32s size = 4; size <= 64; size<<=1) {
            Ipp64u ticksAvx2 = utils::GetMinTicks(100000, AV1PP::adds_nv12_avx2, dst_avx2, pitch, pred, pitch, residU, residV, pitch, size);
            Ipp64u ticksPx   = utils::GetMinTicks(100000, AV1PP::adds_nv12_px,   dst_px,   pitch, pred, pitch, residU, residV, pitch, size);
            printf("%2dx%-2d ref=%-5lld avx2=%-4lld\n", size, size,  ticksPx, ticksAvx2);
        }
    }
}


TEST(optimization, quant) {
    const int srcSize = 32;
    const int dstSize = 32;

    auto src_ptr = utils::MakeAlignedPtr<short>(srcSize * srcSize, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<short>(dstSize * dstSize, utils::AlignAvx2);
    auto dst_tst_ptr = utils::MakeAlignedPtr<short>(dstSize * dstSize, utils::AlignAvx2);
    short *src = src_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_tst = dst_tst_ptr.get();

    AV1Enc::AV1VideoParam par = {};
    par.bitDepthLuma = 8;
    par.bitDepthChroma = 8;
    AV1Enc::InitQuantizer(par);

    srand(0x1234);

    for (Ipp32s qp = AV1Enc::MINQ; qp <= AV1Enc::MAXQ; qp++) {
        Ipp16s *qparam = reinterpret_cast<Ipp16s *>(par.qparamY + qp);
        std::ostringstream buf;
        buf << "Testing qp=" << qp;
        SCOPED_TRACE(buf.str().c_str());

        utils::InitRandomDiffBlock(src, srcSize, srcSize, srcSize, 15);
        utils::InitRandomDiffBlock(dst_ref, dstSize, dstSize, dstSize, 15);
        memcpy(dst_tst, dst_ref, sizeof(Ipp16s) * dstSize * dstSize);

        ASSERT_EQ(AV1PP::quant_px<0>(src, dst_ref, qparam), AV1PP::quant_avx2<0>(src, dst_tst, qparam));
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 4 * 4));

        ASSERT_EQ(AV1PP::quant_px<1>(src, dst_ref, qparam), AV1PP::quant_avx2<1>(src, dst_tst, qparam));
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 8 * 8));

        ASSERT_EQ(AV1PP::quant_px<2>(src, dst_ref, qparam), AV1PP::quant_avx2<2>(src, dst_tst, qparam));
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 16 * 16));

        ASSERT_EQ(AV1PP::quant_px<3>(src, dst_ref, qparam), AV1PP::quant_avx2<3>(src, dst_tst, qparam));
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 32 * 32));
    }

    if (printTicks) {
        Ipp64s t_ref, t_tst;
        Ipp16s *qparam = reinterpret_cast<Ipp16s *>(par.qparamY + 100);
        utils::InitRandomDiffBlock(src, srcSize, srcSize, srcSize, 15);
        t_ref = utils::GetMinTicks(100000,  AV1PP::quant_px<0, short>,   src, dst_ref, qparam);
        t_tst = utils::GetMinTicks(1000000, AV1PP::quant_avx2<0>, src, dst_tst, qparam);
        printf(" 4x4  ref=%-5lld avx2=%-3lld\n", t_ref, t_tst);
        t_ref = utils::GetMinTicks(100000>>1,  AV1PP::quant_px<1, short>,   src, dst_ref, qparam);
        t_tst = utils::GetMinTicks(1000000>>1, AV1PP::quant_avx2<1>, src, dst_tst, qparam);
        printf(" 8x8  ref=%-5lld avx2=%-3lld\n", t_ref, t_tst);
        t_ref = utils::GetMinTicks(100000>>2,  AV1PP::quant_px<2, short>,   src, dst_ref, qparam);
        t_tst = utils::GetMinTicks(1000000>>2, AV1PP::quant_avx2<2>, src, dst_tst, qparam);
        printf("16x16 ref=%-5lld avx2=%-3lld\n", t_ref, t_tst);
        t_ref = utils::GetMinTicks(100000>>3,  AV1PP::quant_px<3, short>,   src, dst_ref, qparam);
        t_tst = utils::GetMinTicks(1000000>>3, AV1PP::quant_avx2<3>, src, dst_tst, qparam);
        printf("32x32 ref=%-5lld avx2=%-3lld\n", t_ref, t_tst);
    }
}


TEST(optimization, dequant) {
    const int srcSize = 32;
    const int dstSize = 32;

    auto src_ptr = utils::MakeAlignedPtr<short>(srcSize * srcSize, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<short>(dstSize * dstSize, utils::AlignAvx2);
    auto dst_tst_ptr = utils::MakeAlignedPtr<short>(dstSize * dstSize, utils::AlignAvx2);
    auto src = src_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_tst = dst_tst_ptr.get();

    AV1Enc::AV1VideoParam par = {};
    par.bitDepthLuma = 8;
    par.bitDepthChroma = 8;
    AV1Enc::InitQuantizer(par);

    srand(0x1234);

    for (Ipp32s qp = AV1Enc::MINQ; qp <= AV1Enc::MAXQ; qp++) {
        const Ipp16s *scale = par.qparamY[qp].dequant;
        std::ostringstream buf;
        buf << "Testing qp=" << qp;
        SCOPED_TRACE(buf.str().c_str());

        utils::InitRandomDiffBlock(src, srcSize, srcSize, srcSize, 8);
        utils::InitRandomDiffBlock(dst_ref, srcSize, dstSize, dstSize, 8);
        memcpy(dst_tst, dst_ref, sizeof(Ipp16s) * dstSize * dstSize);
        AV1PP::dequant_px<0>  (src, dst_ref, scale, 8);
        AV1PP::dequant_avx2<0>(src, dst_tst, scale, 8);
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 4 * 4));

        utils::InitRandomDiffBlock(src, srcSize, srcSize, srcSize, 8);
        utils::InitRandomDiffBlock(dst_ref, srcSize, dstSize, dstSize, 8);
        memcpy(dst_tst, dst_ref, sizeof(Ipp16s) * dstSize * dstSize);
        AV1PP::dequant_px<1>  (src, dst_ref, scale, 8);
        AV1PP::dequant_avx2<1>(src, dst_tst, scale, 8);
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 8 * 8));

        utils::InitRandomDiffBlock(src, srcSize, srcSize, srcSize, 8);
        utils::InitRandomDiffBlock(dst_ref, srcSize, dstSize, dstSize, 8);
        memcpy(dst_tst, dst_ref, sizeof(Ipp16s) * dstSize * dstSize);
        AV1PP::dequant_px<2>  (src, dst_ref, scale, 8);
        AV1PP::dequant_avx2<2>(src, dst_tst, scale, 8);
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 16 * 16));

        utils::InitRandomDiffBlock(src, srcSize, srcSize, srcSize, 8);
        utils::InitRandomDiffBlock(dst_ref, srcSize, dstSize, dstSize, 8);
        memcpy(dst_tst, dst_ref, sizeof(Ipp16s) * dstSize * dstSize);
        AV1PP::dequant_px<3, short>  (src, dst_ref, scale, 8);
        AV1PP::dequant_avx2<3>(src, dst_tst, scale, 8);
        ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 32 * 32));
    }

    if (printTicks) {
        Ipp64s t_ref, t_tst;
        const Ipp16s *scale = par.qparamY[100].dequant;
        utils::InitRandomBlock(src, srcSize, srcSize, srcSize, 8);
        t_ref = utils::GetMinTicks(100000, AV1PP::dequant_px<0, short>,   src, dst_ref, scale, 8);
        t_tst = utils::GetMinTicks(1000000, AV1PP::dequant_avx2<0>, src, dst_tst, scale, 8);
        printf(" 4x4  ref=%-5lld avx2=%-4lld\n", t_ref, t_tst);
        utils::InitRandomBlock(src, srcSize, srcSize, srcSize, 8);
        t_ref = utils::GetMinTicks(100000>>1, AV1PP::dequant_px<1, short>,   src, dst_ref, scale, 8);
        t_tst = utils::GetMinTicks(1000000>>1, AV1PP::dequant_avx2<1>, src, dst_tst, scale, 8);
        printf(" 8x8  ref=%-5lld avx2=%-4lld\n", t_ref, t_tst);
        utils::InitRandomBlock(src, srcSize, srcSize, srcSize, 8);
        t_ref = utils::GetMinTicks(100000>>2, AV1PP::dequant_px<2, short>,   src, dst_ref, scale, 8);
        t_tst = utils::GetMinTicks(1000000>>2, AV1PP::dequant_avx2<2>, src, dst_tst, scale, 8);
        printf("16x16 ref=%-5lld avx2=%-4lld\n", t_ref, t_tst);
        utils::InitRandomBlock(src, srcSize, srcSize, srcSize, 8);
        t_ref = utils::GetMinTicks(100000>>3, AV1PP::dequant_px<3, short>,   src, dst_ref, scale, 8);
        t_tst = utils::GetMinTicks(1000000>>3, AV1PP::dequant_avx2<3>, src, dst_tst, scale, 8);
        printf("32x32 ref=%-5lld avx2=%-4lld\n", t_ref, t_tst);
    }
}


TEST(optimization, quant_dequant) {
    const int srcSize = 32;
    const int dstSize = 32;

    auto src_ref_ptr = utils::MakeAlignedPtr<short>(srcSize * srcSize, utils::AlignAvx2);
    auto src_tst_ptr = utils::MakeAlignedPtr<short>(srcSize * srcSize, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<short>(dstSize * dstSize, utils::AlignAvx2);
    auto dst_tst_ptr = utils::MakeAlignedPtr<short>(dstSize * dstSize, utils::AlignAvx2);
    auto src_ref = src_ref_ptr.get();
    auto src_tst = src_tst_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_tst = dst_tst_ptr.get();

    AV1Enc::AV1VideoParam par = {};
    par.bitDepthLuma = 8;
    par.bitDepthChroma = 8;
    AV1Enc::InitQuantizer(par);

    srand(0x1234);

    for (Ipp32s qp = AV1Enc::MINQ; qp <= AV1Enc::MAXQ; qp++) {
        Ipp16s *qparam = reinterpret_cast<Ipp16s *>(par.qparamY + qp);
        std::ostringstream buf;
        buf << "Testing qp=" << qp;
        SCOPED_TRACE(buf.str().c_str());

        utils::InitRandomBlock(src_ref, srcSize, srcSize, srcSize, 15);
        utils::InitRandomBlock(dst_ref, dstSize, dstSize, dstSize, 15);
        memcpy(src_tst, src_ref, sizeof(Ipp16s) * srcSize * srcSize);
        memcpy(dst_tst, dst_ref, sizeof(Ipp16s) * dstSize * dstSize);

        { SCOPED_TRACE("Testing 4x4");
            ASSERT_EQ(AV1PP::quant_dequant_px<0>(src_ref, dst_ref, qparam), AV1PP::quant_dequant_avx2<0>(src_tst, dst_tst, qparam));
            ASSERT_EQ(0, memcmp(src_ref, src_tst, sizeof(Ipp16s) * 32 * 32));
            ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 32 * 32));
        }

        { SCOPED_TRACE("Testing 8x8");
            ASSERT_EQ(AV1PP::quant_dequant_px<1>(src_ref, dst_ref, qparam), AV1PP::quant_dequant_avx2<1>(src_tst, dst_tst, qparam));
            ASSERT_EQ(0, memcmp(src_ref, src_tst, sizeof(Ipp16s) * 32 * 32));
            ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 32 * 32));
        }

        { SCOPED_TRACE("Testing 16x16");
            ASSERT_EQ(AV1PP::quant_dequant_px<2>(src_ref, dst_ref, qparam), AV1PP::quant_dequant_avx2<2>(src_tst, dst_tst, qparam));
            ASSERT_EQ(0, memcmp(src_ref, src_tst, sizeof(Ipp16s) * 32 * 32));
            ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 32 * 32));
        }

        { SCOPED_TRACE("Testing 32x32");
            ASSERT_EQ(AV1PP::quant_dequant_px<3>(src_ref, dst_ref, qparam), AV1PP::quant_dequant_avx2<3>(src_tst, dst_tst, qparam));
            ASSERT_EQ(0, memcmp(src_ref, src_tst, sizeof(Ipp16s) * 32 * 32));
            ASSERT_EQ(0, memcmp(dst_ref, dst_tst, sizeof(Ipp16s) * 32 * 32));
        }
    }

    if (printTicks) {
        Ipp64s t_ref, t_tst;
        Ipp16s *qparam = reinterpret_cast<Ipp16s *>(par.qparamY + 100);
        utils::InitRandomBlock(src_ref, srcSize, srcSize, srcSize, 16);
        memcpy(src_tst, src_ref, sizeof(Ipp16s) * srcSize * srcSize);
        t_ref = utils::GetMinTicks(100000,  AV1PP::quant_dequant_px<0>,   src_ref, dst_ref, qparam);
        t_tst = utils::GetMinTicks(1000000, AV1PP::quant_dequant_avx2<0>, src_ref, dst_tst, qparam);
        printf(" 4x4  ref=%-5lld avx2=%-3lld\n", t_ref, t_tst);
        t_ref = utils::GetMinTicks(100000>>1,  AV1PP::quant_dequant_px<1>,   src_ref, dst_ref, qparam);
        t_tst = utils::GetMinTicks(1000000>>1, AV1PP::quant_dequant_avx2<1>, src_ref, dst_tst, qparam);
        printf(" 8x8  ref=%-5lld avx2=%-3lld\n", t_ref, t_tst);
        t_ref = utils::GetMinTicks(100000>>2,  AV1PP::quant_dequant_px<2>,   src_ref, dst_ref, qparam);
        t_tst = utils::GetMinTicks(1000000>>2, AV1PP::quant_dequant_avx2<2>, src_ref, dst_tst, qparam);
        printf("16x16 ref=%-5lld avx2=%-3lld\n", t_ref, t_tst);
        t_ref = utils::GetMinTicks(100000>>3,  AV1PP::quant_dequant_px<3>,   src_ref, dst_ref, qparam);
        t_tst = utils::GetMinTicks(1000000>>3, AV1PP::quant_dequant_avx2<3>, src_ref, dst_tst, qparam);
        printf("32x32 ref=%-5lld avx2=%-3lld\n", t_ref, t_tst);
    }
}


TEST(optimization, compute_rscs) {
    const Ipp32s padding = utils::AlignAvx2;
    Ipp32s cusize = 64;
    Ipp32s pitch  = std::max(256, cusize+3*padding);

    Ipp32s rs_px[256] = {}, rs_avx2[256] = {};
    Ipp32s cs_px[256] = {}, cs_avx2[256] = {};

    auto srcBuf = utils::MakeAlignedPtr<unsigned char>(pitch*(cusize+2*padding), utils::AlignAvx2);
    auto src = srcBuf.get() + padding + padding * pitch;

    srand(0x1234);
    utils::InitRandomBlock(srcBuf.get(), pitch, cusize+2*padding, cusize+2*padding, 8);

    AV1PP::compute_rscs_px  (src, pitch, rs_px,   cs_px,   16, cusize, cusize);
    AV1PP::compute_rscs_avx2(src, pitch, rs_avx2, cs_avx2, 16, cusize, cusize);
    ASSERT_EQ(0, memcmp(rs_px, rs_avx2, sizeof(rs_px)));
    ASSERT_EQ(0, memcmp(cs_px, cs_avx2, sizeof(cs_px)));
    if (printTicks) {
        Ipp64s tpx   = utils::GetMinTicks(100000, AV1PP::compute_rscs_px,   src, pitch, rs_px,   cs_px,   16, cusize, cusize);
        Ipp64s tavx2 = utils::GetMinTicks(100000, AV1PP::compute_rscs_avx2, src, pitch, rs_avx2, cs_avx2, 16, cusize, cusize);
        printf("ref=%I64d avx2=%I64d\n", tpx, tavx2);
    }
}


TEST(optimization, ftransform)
{
    using namespace AV1Enc;

    const int max_size = 32;
    const int pitch = 64;
    const int srcSize = max_size * pitch;
    const int dstSize = max_size * max_size;

    auto src_ptr     = utils::MakeAlignedPtr<short>(srcSize, utils::AlignAvx2);       // transforms assume SSE/AVX2 aligned inputs
    auto dst_ref_ptr = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);
    auto src = src_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    typedef void(*fptr_t)(const short*, short*, int);
    using AV1PP::ftransform_av1_px;
    using AV1PP::ftransform_av1_avx2;

    fptr_t fptr_ref[5][4] = {
        {ftransform_av1_px<TX_4X4,DCT_DCT>,   ftransform_av1_px<TX_8X8,DCT_DCT>,   ftransform_av1_px<TX_16X16,DCT_DCT>,   ftransform_av1_px<TX_32X32,DCT_DCT> },
        {ftransform_av1_px<TX_4X4,ADST_DCT>,  ftransform_av1_px<TX_8X8,ADST_DCT>,  ftransform_av1_px<TX_16X16,ADST_DCT>,  nullptr                             },
        {ftransform_av1_px<TX_4X4,DCT_ADST>,  ftransform_av1_px<TX_8X8,DCT_ADST>,  ftransform_av1_px<TX_16X16,DCT_ADST>,  nullptr                             },
        {ftransform_av1_px<TX_4X4,ADST_ADST>, ftransform_av1_px<TX_8X8,ADST_ADST>, ftransform_av1_px<TX_16X16,ADST_ADST>, nullptr                             },
        {ftransform_av1_px<TX_4X4,IDTX>,      ftransform_av1_px<TX_8X8,IDTX>,      ftransform_av1_px<TX_16X16,IDTX>,      ftransform_av1_px<TX_32X32,IDTX>    },
    };

    fptr_t fptr_avx[5][4] = {
        {ftransform_av1_avx2<TX_4X4,DCT_DCT>,   ftransform_av1_avx2<TX_8X8,DCT_DCT>,   ftransform_av1_avx2<TX_16X16,DCT_DCT>,   ftransform_av1_avx2<TX_32X32,DCT_DCT> },
        {ftransform_av1_avx2<TX_4X4,ADST_DCT>,  ftransform_av1_avx2<TX_8X8,ADST_DCT>,  ftransform_av1_avx2<TX_16X16,ADST_DCT>,  nullptr                               },
        {ftransform_av1_avx2<TX_4X4,DCT_ADST>,  ftransform_av1_avx2<TX_8X8,DCT_ADST>,  ftransform_av1_avx2<TX_16X16,DCT_ADST>,  nullptr                               },
        {ftransform_av1_avx2<TX_4X4,ADST_ADST>, ftransform_av1_avx2<TX_8X8,ADST_ADST>, ftransform_av1_avx2<TX_16X16,ADST_ADST>, nullptr                               },
        {ftransform_av1_avx2<TX_4X4,IDTX>,      ftransform_av1_avx2<TX_8X8,IDTX>,      ftransform_av1_avx2<TX_16X16,IDTX>,      ftransform_av1_avx2<TX_32X32,IDTX>    },
    };

    const char *type2str[] = { "DCT_DCT", "ADST_DCT", "DCT_ADST", "ADST_ADST", "IDTX" };

    srand(0x1234);

    for (int size = 0; size < 4; size++) {
        for (int type = 0; type < 5; type++) {
            if (!fptr_ref[type][size] || !fptr_avx[type][size])
                continue;
            if (type == 4 && (size == TX_4X4 || size == TX_16X16))
                continue; // avx2-version is optimized and not b2b with c-version

            const int w = 4 << size;
            const int h = 4 << size;

            std::ostringstream buf;
            buf << "Testing " << w << "x" << h << " " << type2str[type];
            SCOPED_TRACE(buf.str().c_str());

            utils::InitConstBlock(src, pitch, w, h, (1 << 8) - 1);
            fptr_ref[type][size](src, dst_ref, pitch);
            fptr_avx[type][size](src, dst_avx, pitch);
            EXPECT_TRUE(utils::BuffersAreEqual(dst_ref, dst_avx, w * h));

            utils::InitConstBlock(src, pitch, w, h, -(1 << 8));
            fptr_ref[type][size](src, dst_ref, pitch);
            fptr_avx[type][size](src, dst_avx, pitch);
            EXPECT_TRUE(utils::BuffersAreEqual(dst_ref, dst_avx, w * h));

            utils::InitRandomDiffBlock(src, pitch, w, h, 8);
            fptr_ref[type][size](src, dst_ref, pitch);
            fptr_avx[type][size](src, dst_avx, pitch);
            EXPECT_TRUE(utils::BuffersAreEqual(dst_ref, dst_avx, w * h));
        }
    }

    if (printTicks) {
        utils::InitRandomDiffBlock(src, pitch, 32, 32, 8);
        for (int size = 0; size < 4; size++) {
            for (int type = 0; type < 5; type++) {
                if (!fptr_ref[type][size] || !fptr_avx[type][size])
                    continue;
                const int w = 4 << size;
                const int h = 4 << size;
                const int64_t tref = utils::GetMinTicks(100000, fptr_ref[type][size], src, dst_ref, pitch);
                const int64_t tavx = utils::GetMinTicks(100000, fptr_avx[type][size], src, dst_avx, pitch);
                printf("%2dx%-2d %-9s ref=%-5lld avx2=%-5lld\n", w, h, type2str[type], tref, tavx);
            }
        }
    }
}


TEST(optimization, itransform) {
    using namespace AV1Enc;

    const int pitch = 32;
    const int srcSize = 32*32;  // max 32x32
    const int dstSize = 64*64;  // alloc more space to confirm no writing outside of dst block

    auto src_ptr      = utils::MakeAlignedPtr<Ipp16s>(srcSize, utils::AlignAvx2);       // transforms assume SSE/AVX2 aligned inputs
    auto dst_px_ptr   = utils::MakeAlignedPtr<Ipp16s>(dstSize, utils::AlignAvx2);
    auto dst_avx2_ptr = utils::MakeAlignedPtr<Ipp16s>(dstSize, utils::AlignAvx2);
    auto src = src_ptr.get();
    auto dst_px = dst_px_ptr.get();
    auto dst_avx2 = dst_avx2_ptr.get();

    srand(0x1234);

    const int MAX_ITER_COUNT = 100;
    for (int iter = 0; iter < MAX_ITER_COUNT; iter++) {
        utils::InitRandomDiffBlock(src, 32, 32, 32, 7);
        for (Ipp32s i = 0; i < srcSize; i++)
            src[i] = i;
        utils::InitRandomBlock(dst_px, 64, 64, 64, 8);
        memcpy(dst_avx2, dst_px, sizeof(*dst_px)*dstSize);

        AV1PP::itransform_av1_px  <TX_4X4, DCT_DCT>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_4X4, DCT_DCT>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_av1_px  <TX_4X4, ADST_DCT>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_4X4, ADST_DCT>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_av1_px  <TX_4X4, DCT_ADST>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_4X4, DCT_ADST>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_av1_px  <TX_4X4, ADST_ADST>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_4X4, ADST_ADST>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));

        AV1PP::itransform_av1_px  <TX_8X8, DCT_DCT>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_8X8, DCT_DCT>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_av1_px  <TX_8X8, ADST_DCT>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_8X8, ADST_DCT>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_av1_px  <TX_8X8, DCT_ADST>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_8X8, DCT_ADST>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_av1_px  <TX_8X8, ADST_ADST>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_8X8, ADST_ADST>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));

        AV1PP::itransform_av1_px  <TX_16X16, DCT_DCT>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_16X16, DCT_DCT>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_av1_px  <TX_16X16, ADST_DCT>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_16X16, ADST_DCT>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_av1_px  <TX_16X16, DCT_ADST>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_16X16, DCT_ADST>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_av1_px  <TX_16X16, ADST_ADST>(src, dst_px,   pitch, 8);
        AV1PP::itransform_av1_avx2<TX_16X16, ADST_ADST>(src, dst_avx2, pitch, 8);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));

        //AV1PP::itransform_av1_px  <TX_32X32, DCT_DCT>(src, dst_px,   pitch, 8);
        //AV1PP::itransform_av1_avx2<TX_32X32, DCT_DCT>(src, dst_avx2, pitch, 8);
        //EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
    }

    if (printTicks) {
        Ipp64s tpx, tavx2;
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_4X4, DCT_DCT>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_4X4, DCT_DCT>, src, dst_avx2, pitch, 8);
        printf(" 4x4  DCT_DCT   ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_4X4, DCT_ADST>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_4X4, DCT_ADST>, src, dst_avx2, pitch, 8);
        printf(" 4x4  DCT_ADST  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_4X4, ADST_DCT>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_4X4, ADST_DCT>, src, dst_avx2, pitch, 8);
        printf(" 4x4  ADST_DCT  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_4X4, ADST_ADST>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_4X4, ADST_ADST>, src, dst_avx2, pitch, 8);
        printf(" 4x4  ADST_ADST ref=%-5lld avx2=%-5lld\n", tpx, tavx2);

        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_8X8, DCT_DCT>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_8X8, DCT_DCT>, src, dst_avx2, pitch, 8);
        printf(" 8x8  DCT_DCT   ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_8X8, DCT_ADST>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_8X8, DCT_ADST>, src, dst_avx2, pitch, 8);
        printf(" 8x8  DCT_ADST  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_8X8, ADST_DCT>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_8X8, ADST_DCT>, src, dst_avx2, pitch, 8);
        printf(" 8x8  ADST_DCT  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_8X8, ADST_ADST>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_8X8, ADST_ADST>, src, dst_avx2, pitch, 8);
        printf(" 8x8  ADST_ADST ref=%-5lld avx2=%-5lld\n", tpx, tavx2);

        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_16X16, DCT_DCT>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_16X16, DCT_DCT>, src, dst_avx2, pitch, 8);
        printf("16x16 DCT_DCT   ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_16X16, DCT_ADST>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_16X16, DCT_ADST>, src, dst_avx2, pitch, 8);
        printf("16x16 DCT_ADST  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_16X16, ADST_DCT>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_16X16, ADST_DCT>, src, dst_avx2, pitch, 8);
        printf("16x16 ADST_DCT  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_16X16, ADST_ADST>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_16X16, ADST_ADST>, src, dst_avx2, pitch, 8);
        printf("16x16 ADST_ADST ref=%-5lld avx2=%-5lld\n", tpx, tavx2);

        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_av1_px  <TX_32X32, DCT_DCT>, src, dst_px,   pitch, 8);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_av1_avx2<TX_32X32, DCT_DCT>, src, dst_avx2, pitch, 8);
        printf("32x32 DCT_DCT   ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
    }
}


TEST(optimization, itransform_add) {
    using namespace AV1Enc;

    const int pitch = 32;
    const int srcSize = 32*32;  // max 32x32
    const int dstSize = 64*64;  // alloc more space to confirm no writing outside of dst block

    auto src_ptr      = utils::MakeAlignedPtr<short>(srcSize, utils::AlignAvx2);       // transforms assume SSE/AVX2 aligned inputs
    auto dst_init_ptr = utils::MakeAlignedPtr<Ipp8u>(dstSize, utils::AlignAvx2);
    auto dst_px_ptr   = utils::MakeAlignedPtr<Ipp8u>(dstSize, utils::AlignAvx2);
    auto dst_avx2_ptr = utils::MakeAlignedPtr<Ipp8u>(dstSize, utils::AlignAvx2);
    auto src      = src_ptr.get();
    auto dst_init = dst_init_ptr.get();
    auto dst_px   = dst_px_ptr.get();
    auto dst_avx2 = dst_avx2_ptr.get();

    srand(0x1234);

    const int MAX_ITER_COUNT = 100;
    for (int iter = 0; iter < MAX_ITER_COUNT; iter++) {
        utils::InitRandomDiffBlock(src, 32, 32, 32, 7);
        utils::InitRandomBlock(dst_init, 64, 64, 64, 8);

        memcpy(dst_px,   dst_init, sizeof(*dst_init)*dstSize);
        memcpy(dst_avx2, dst_init, sizeof(*dst_init)*dstSize);
        AV1PP::itransform_add_av1_px  <TX_4X4, DCT_DCT>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_4X4, DCT_DCT>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_add_av1_px  <TX_4X4, ADST_DCT>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_4X4, ADST_DCT>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_add_av1_px  <TX_4X4, DCT_ADST>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_4X4, DCT_ADST>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_add_av1_px  <TX_4X4, ADST_ADST>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_4X4, ADST_ADST>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));

        memcpy(dst_px,   dst_init, sizeof(*dst_init)*dstSize);
        memcpy(dst_avx2, dst_init, sizeof(*dst_init)*dstSize);
        AV1PP::itransform_add_av1_px  <TX_8X8, DCT_DCT>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_8X8, DCT_DCT>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_add_av1_px  <TX_8X8, ADST_DCT>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_8X8, ADST_DCT>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_add_av1_px  <TX_8X8, DCT_ADST>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_8X8, DCT_ADST>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_add_av1_px  <TX_8X8, ADST_ADST>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_8X8, ADST_ADST>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));

        memcpy(dst_px,   dst_init, sizeof(*dst_init)*dstSize);
        memcpy(dst_avx2, dst_init, sizeof(*dst_init)*dstSize);
        AV1PP::itransform_add_av1_px  <TX_16X16, DCT_DCT>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_16X16, DCT_DCT>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_add_av1_px  <TX_16X16, ADST_DCT>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_16X16, ADST_DCT>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_add_av1_px  <TX_16X16, DCT_ADST>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_16X16, DCT_ADST>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
        AV1PP::itransform_add_av1_px  <TX_16X16, ADST_ADST>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_16X16, ADST_ADST>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));

        memcpy(dst_px,   dst_init, sizeof(*dst_init)*dstSize);
        memcpy(dst_avx2, dst_init, sizeof(*dst_init)*dstSize);
        AV1PP::itransform_add_av1_px  <TX_32X32, DCT_DCT>(src, dst_px,   pitch);
        AV1PP::itransform_add_av1_avx2<TX_32X32, DCT_DCT>(src, dst_avx2, pitch);
        EXPECT_EQ(0, memcmp(dst_px, dst_avx2, sizeof(*dst_px) * dstSize));
    }

    if (printTicks) {
        Ipp64s tpx, tavx2;
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_4X4, DCT_DCT>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_4X4, DCT_DCT>, src, dst_avx2, pitch);
        printf(" 4x4  DCT_DCT   ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_4X4, DCT_ADST>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_4X4, DCT_ADST>, src, dst_avx2, pitch);
        printf(" 4x4  DCT_ADST  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_4X4, ADST_DCT>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_4X4, ADST_DCT>, src, dst_avx2, pitch);
        printf(" 4x4  ADST_DCT  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_4X4, ADST_ADST>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_4X4, ADST_ADST>, src, dst_avx2, pitch);
        printf(" 4x4  ADST_ADST ref=%-5lld avx2=%-5lld\n", tpx, tavx2);

        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_8X8, DCT_DCT>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_8X8, DCT_DCT>, src, dst_avx2, pitch);
        printf(" 8x8  DCT_DCT   ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_8X8, DCT_ADST>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_8X8, DCT_ADST>, src, dst_avx2, pitch);
        printf(" 8x8  DCT_ADST  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_8X8, ADST_DCT>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_8X8, ADST_DCT>, src, dst_avx2, pitch);
        printf(" 8x8  ADST_DCT  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_8X8, ADST_ADST>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_8X8, ADST_ADST>, src, dst_avx2, pitch);
        printf(" 8x8  ADST_ADST ref=%-5lld avx2=%-5lld\n", tpx, tavx2);

        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_16X16, DCT_DCT>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_16X16, DCT_DCT>, src, dst_avx2, pitch);
        printf("16x16 DCT_DCT   ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_16X16, DCT_ADST>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_16X16, DCT_ADST>, src, dst_avx2, pitch);
        printf("16x16 DCT_ADST  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_16X16, ADST_DCT>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_16X16, ADST_DCT>, src, dst_avx2, pitch);
        printf("16x16 ADST_DCT  ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_16X16, ADST_ADST>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_16X16, ADST_ADST>, src, dst_avx2, pitch);
        printf("16x16 ADST_ADST ref=%-5lld avx2=%-5lld\n", tpx, tavx2);

        tpx   = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_px  <TX_32X32, DCT_DCT>, src, dst_px,   pitch);
        tavx2 = utils::GetMinTicks(100000, AV1PP::itransform_add_av1_avx2<TX_32X32, DCT_DCT>, src, dst_avx2, pitch);
        printf("32x32 DCT_DCT   ref=%-5lld avx2=%-5lld\n", tpx, tavx2);
    }
}


TEST(optimization, predict_intra_dc_av1)
{
    const int MAX_WIDTH = 32;
    const int MAX_HEIGHT = 32;
    const int top_pels_size = 2 * MAX_WIDTH + utils::AlignAvx2;
    const int left_pels_size = 2 * MAX_HEIGHT + utils::AlignAvx2;
    const int pitch = 64;
    const int dst_size = MAX_HEIGHT * pitch;

    auto top_pels_ptr = utils::MakeAlignedPtr<uint8_t>(top_pels_size, utils::AlignAvx2);
    auto left_pels_ptr = utils::MakeAlignedPtr<uint8_t>(left_pels_size, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<uint8_t>(dst_size, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<uint8_t>(dst_size, utils::AlignAvx2);
    auto top_pels = top_pels_ptr.get();
    auto left_pels = left_pels_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    srand(0x1234);

    using AV1PP::predict_intra_dc_av1_px;
    using AV1PP::predict_intra_dc_av1_avx2;
    typedef void (*fptr_t)(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int);
    const fptr_t fptr_ref[4][2][2] = {
        {{predict_intra_dc_av1_px<0,0,0>, predict_intra_dc_av1_px<0,0,1>}, {predict_intra_dc_av1_px<0,1,0>, predict_intra_dc_av1_px<0,1,1>}},
        {{predict_intra_dc_av1_px<1,0,0>, predict_intra_dc_av1_px<1,0,1>}, {predict_intra_dc_av1_px<1,1,0>, predict_intra_dc_av1_px<1,1,1>}},
        {{predict_intra_dc_av1_px<2,0,0>, predict_intra_dc_av1_px<2,0,1>}, {predict_intra_dc_av1_px<2,1,0>, predict_intra_dc_av1_px<2,1,1>}},
        {{predict_intra_dc_av1_px<3,0,0>, predict_intra_dc_av1_px<3,0,1>}, {predict_intra_dc_av1_px<3,1,0>, predict_intra_dc_av1_px<3,1,1>}},
    };
    const fptr_t fptr_avx[4][2][2] = {
        {{predict_intra_dc_av1_avx2<0,0,0>, predict_intra_dc_av1_avx2<0,0,1>}, {predict_intra_dc_av1_avx2<0,1,0>, predict_intra_dc_av1_avx2<0,1,1>}},
        {{predict_intra_dc_av1_avx2<1,0,0>, predict_intra_dc_av1_avx2<1,0,1>}, {predict_intra_dc_av1_avx2<1,1,0>, predict_intra_dc_av1_avx2<1,1,1>}},
        {{predict_intra_dc_av1_avx2<2,0,0>, predict_intra_dc_av1_avx2<2,0,1>}, {predict_intra_dc_av1_avx2<2,1,0>, predict_intra_dc_av1_avx2<2,1,1>}},
        {{predict_intra_dc_av1_avx2<3,0,0>, predict_intra_dc_av1_avx2<3,0,1>}, {predict_intra_dc_av1_avx2<3,1,0>, predict_intra_dc_av1_avx2<3,1,1>}},
    };

    for (int size = 0; size < 4; size++) {
        const int w = 4 << size;
        const int h = 4 << size;
        for (int have_left = 0; have_left < 2; have_left++) {
            for (int have_top = 0; have_top < 2; have_top++) {
                for (int ntries = 10; ntries > 0; ntries--) {
                    std::ostringstream buf;
                    buf << "Testing size=" << w << "x" << h << " have_left=" << have_left << " have_top=" << have_top;
                    SCOPED_TRACE(buf.str().c_str());
                    utils::InitRandomBuffer(top_pels, top_pels_size, 8);
                    utils::InitRandomBuffer(left_pels, left_pels_size, 8);
                    fptr_ref[size][have_left][have_top](top_pels + 32, left_pels + 32, dst_ref, pitch, 0, 0, 0);
                    fptr_avx[size][have_left][have_top](top_pels + 32, left_pels + 32, dst_avx, pitch, 0, 0, 0);
                    ASSERT_TRUE(utils::BlocksAreEqual(dst_ref, pitch, dst_avx, pitch, w, h));
                }
                if (printTicks) {
                    const uint64_t tref = utils::GetMinTicks(2000000 >> size, fptr_ref[size][have_left][have_top], top_pels + 32, left_pels + 32, dst_ref, pitch, 0, 0, 0);
                    const uint64_t tavx = utils::GetMinTicks(2000000 >> size, fptr_avx[size][have_left][have_top], top_pels + 32, left_pels + 32, dst_avx, pitch, 0, 0, 0);
                    printf("%2dx%-2d have_left=%d have_top=%d ref=%-4lld avx=%-4lld\n", w, h, have_left, have_top, tref, tavx);
                }
            }
        }
    }
}


TEST(optimization, predict_intra_dc_av1_hbd)
{
    const int MAX_WIDTH = 32;
    const int MAX_HEIGHT = 32;
    const int top_pels_size = 2 * MAX_WIDTH + utils::AlignAvx2;
    const int left_pels_size = 2 * MAX_HEIGHT + utils::AlignAvx2;
    const int pitch = 64;
    const int dst_size = MAX_HEIGHT * pitch;

    auto top_pels_ptr = utils::MakeAlignedPtr<uint16_t>(top_pels_size, utils::AlignAvx2);
    auto left_pels_ptr = utils::MakeAlignedPtr<uint16_t>(left_pels_size, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<uint16_t>(dst_size, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<uint16_t>(dst_size, utils::AlignAvx2);
    auto top_pels = top_pels_ptr.get();
    auto left_pels = left_pels_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    srand(0x1234);

    using AV1PP::predict_intra_dc_av1_px;
    using AV1PP::predict_intra_dc_av1_avx2;
    typedef void(*fptr_t)(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int);
    const fptr_t fptr_ref[4][2][2] = {
        {{predict_intra_dc_av1_px<0,0,0>, predict_intra_dc_av1_px<0,0,1>}, {predict_intra_dc_av1_px<0,1,0>, predict_intra_dc_av1_px<0,1,1>}},
        {{predict_intra_dc_av1_px<1,0,0>, predict_intra_dc_av1_px<1,0,1>}, {predict_intra_dc_av1_px<1,1,0>, predict_intra_dc_av1_px<1,1,1>}},
        {{predict_intra_dc_av1_px<2,0,0>, predict_intra_dc_av1_px<2,0,1>}, {predict_intra_dc_av1_px<2,1,0>, predict_intra_dc_av1_px<2,1,1>}},
        {{predict_intra_dc_av1_px<3,0,0>, predict_intra_dc_av1_px<3,0,1>}, {predict_intra_dc_av1_px<3,1,0>, predict_intra_dc_av1_px<3,1,1>}},
    };
    const fptr_t fptr_avx[4][2][2] = {
        {{predict_intra_dc_av1_avx2<0,0,0>, predict_intra_dc_av1_avx2<0,0,1>}, {predict_intra_dc_av1_avx2<0,1,0>, predict_intra_dc_av1_avx2<0,1,1>}},
        {{predict_intra_dc_av1_avx2<1,0,0>, predict_intra_dc_av1_avx2<1,0,1>}, {predict_intra_dc_av1_avx2<1,1,0>, predict_intra_dc_av1_avx2<1,1,1>}},
        {{predict_intra_dc_av1_avx2<2,0,0>, predict_intra_dc_av1_avx2<2,0,1>}, {predict_intra_dc_av1_avx2<2,1,0>, predict_intra_dc_av1_avx2<2,1,1>}},
        {{predict_intra_dc_av1_avx2<3,0,0>, predict_intra_dc_av1_avx2<3,0,1>}, {predict_intra_dc_av1_avx2<3,1,0>, predict_intra_dc_av1_avx2<3,1,1>}},
    };

    for (int size = 0; size < 4; size++) {
        const int w = 4 << size;
        const int h = 4 << size;
        for (int have_left = 0; have_left < 2; have_left++) {
            for (int have_top = 0; have_top < 2; have_top++) {
                for (int ntries = 10; ntries > 0; ntries--) {
                    std::ostringstream buf;
                    buf << "Testing size=" << w << "x" << h << " have_left=" << have_left << " have_top=" << have_top;
                    SCOPED_TRACE(buf.str().c_str());
                    utils::InitRandomBuffer(top_pels, top_pels_size, 10);
                    utils::InitRandomBuffer(left_pels, left_pels_size, 10);
                    fptr_ref[size][have_left][have_top](top_pels + 32, left_pels + 32, dst_ref, pitch, 0, 0, 0);
                    fptr_avx[size][have_left][have_top](top_pels + 32, left_pels + 32, dst_avx, pitch, 0, 0, 0);
                    ASSERT_TRUE(utils::BlocksAreEqual(dst_ref, pitch, dst_avx, pitch, w, h));
                }
                if (printTicks) {
                    const uint64_t tref = utils::GetMinTicks(2000000 >> size, fptr_ref[size][have_left][have_top], top_pels + 32, left_pels + 32, dst_ref, pitch, 0, 0, 0);
                    const uint64_t tavx = utils::GetMinTicks(2000000 >> size, fptr_avx[size][have_left][have_top], top_pels + 32, left_pels + 32, dst_avx, pitch, 0, 0, 0);
                    printf("%2dx%-2d have_left=%d have_top=%d ref=%-4lld avx=%-4lld\n", w, h, have_left, have_top, tref, tavx);
                }
            }
        }
    }
}


TEST(optimization, predict_intra_nv12_dc_av1)
{
    const int MAX_WIDTH = 32;
    const int MAX_HEIGHT = 32;
    const int top_pels_size = 4 * MAX_WIDTH + utils::AlignAvx2;
    const int left_pels_size = 2 * MAX_HEIGHT + utils::AlignAvx2;
    const int pitch = 128;
    const int dst_size = MAX_HEIGHT * pitch;

    auto top_pels_ptr = utils::MakeAlignedPtr<uint8_t>(top_pels_size, utils::AlignAvx2);
    auto left_pels_ptr = utils::MakeAlignedPtr<uint8_t>(left_pels_size, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<uint8_t>(dst_size, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<uint8_t>(dst_size, utils::AlignAvx2);
    auto top_pels = top_pels_ptr.get();
    auto left_pels = left_pels_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    srand(0x1234);

    using AV1PP::predict_intra_nv12_dc_av1_px;
    using AV1PP::predict_intra_nv12_dc_av1_avx2;
    typedef void(*fptr_t)(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int);
    const fptr_t fptr_ref[4][2][2] = {
        {{predict_intra_nv12_dc_av1_px<0,0,0>, predict_intra_nv12_dc_av1_px<0,0,1>}, {predict_intra_nv12_dc_av1_px<0,1,0>, predict_intra_nv12_dc_av1_px<0,1,1>}},
        {{predict_intra_nv12_dc_av1_px<1,0,0>, predict_intra_nv12_dc_av1_px<1,0,1>}, {predict_intra_nv12_dc_av1_px<1,1,0>, predict_intra_nv12_dc_av1_px<1,1,1>}},
        {{predict_intra_nv12_dc_av1_px<2,0,0>, predict_intra_nv12_dc_av1_px<2,0,1>}, {predict_intra_nv12_dc_av1_px<2,1,0>, predict_intra_nv12_dc_av1_px<2,1,1>}},
        {{predict_intra_nv12_dc_av1_px<3,0,0>, predict_intra_nv12_dc_av1_px<3,0,1>}, {predict_intra_nv12_dc_av1_px<3,1,0>, predict_intra_nv12_dc_av1_px<3,1,1>}},
    };
    const fptr_t fptr_avx[4][2][2] = {
        {{predict_intra_nv12_dc_av1_avx2<0,0,0>, predict_intra_nv12_dc_av1_avx2<0,0,1>}, {predict_intra_nv12_dc_av1_avx2<0,1,0>, predict_intra_nv12_dc_av1_avx2<0,1,1>}},
        {{predict_intra_nv12_dc_av1_avx2<1,0,0>, predict_intra_nv12_dc_av1_avx2<1,0,1>}, {predict_intra_nv12_dc_av1_avx2<1,1,0>, predict_intra_nv12_dc_av1_avx2<1,1,1>}},
        {{predict_intra_nv12_dc_av1_avx2<2,0,0>, predict_intra_nv12_dc_av1_avx2<2,0,1>}, {predict_intra_nv12_dc_av1_avx2<2,1,0>, predict_intra_nv12_dc_av1_avx2<2,1,1>}},
        {{predict_intra_nv12_dc_av1_avx2<3,0,0>, predict_intra_nv12_dc_av1_avx2<3,0,1>}, {predict_intra_nv12_dc_av1_avx2<3,1,0>, predict_intra_nv12_dc_av1_avx2<3,1,1>}},
    };

    for (int size = 0; size < 4; size++) {
        const int w = 4 << size;
        const int h = 4 << size;
        for (int have_left = 0; have_left < 2; have_left++) {
            for (int have_top = 0; have_top < 2; have_top++) {
                for (int ntries = 10; ntries > 0; ntries--) {
                    std::ostringstream buf;
                    buf << "Testing size=" << w << "x" << h << " have_left=" << have_left << " have_top=" << have_top;
                    SCOPED_TRACE(buf.str().c_str());
                    utils::InitRandomBuffer(top_pels, top_pels_size, 8);
                    utils::InitRandomBuffer(left_pels, left_pels_size, 8);
                    fptr_ref[size][have_left][have_top](top_pels + 32, left_pels + 32, dst_ref, pitch, 0, 0, 0);
                    fptr_avx[size][have_left][have_top](top_pels + 32, left_pels + 32, dst_avx, pitch, 0, 0, 0);
                    ASSERT_TRUE(utils::BlocksAreEqual(dst_ref, pitch, dst_avx, pitch, 2 * w, h));
                }
                if (printTicks) {
                    const uint64_t tref = utils::GetMinTicks(2000000 >> size, fptr_ref[size][have_left][have_top], top_pels + 32, left_pels + 32, dst_ref, pitch, 0, 0, 0);
                    const uint64_t tavx = utils::GetMinTicks(2000000 >> size, fptr_avx[size][have_left][have_top], top_pels + 32, left_pels + 32, dst_avx, pitch, 0, 0, 0);
                    printf("%2dx%-2d have_left=%d have_top=%d ref=%-4lld avx=%-4lld\n", w, h, have_left, have_top, tref, tavx);
                }
            }
        }
    }
}


TEST(optimization, predict_intra_nv12_dc_av1_hbd)
{
    const int MAX_WIDTH = 32;
    const int MAX_HEIGHT = 32;
    const int top_pels_size = 4 * MAX_WIDTH + utils::AlignAvx2;
    const int left_pels_size = 2 * MAX_HEIGHT + utils::AlignAvx2;
    const int pitch = 128;
    const int dst_size = MAX_HEIGHT * pitch;

    auto top_pels_ptr = utils::MakeAlignedPtr<uint16_t>(top_pels_size, utils::AlignAvx2);
    auto left_pels_ptr = utils::MakeAlignedPtr<uint16_t>(left_pels_size, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<uint16_t>(dst_size, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<uint16_t>(dst_size, utils::AlignAvx2);
    auto top_pels = top_pels_ptr.get();
    auto left_pels = left_pels_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    srand(0x1234);

    using AV1PP::predict_intra_nv12_dc_av1_px;
    using AV1PP::predict_intra_nv12_dc_av1_avx2;
    typedef void(*fptr_t)(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int);
    const fptr_t fptr_ref[4][2][2] = {
        {{predict_intra_nv12_dc_av1_px<0,0,0>, predict_intra_nv12_dc_av1_px<0,0,1>}, {predict_intra_nv12_dc_av1_px<0,1,0>, predict_intra_nv12_dc_av1_px<0,1,1>}},
        {{predict_intra_nv12_dc_av1_px<1,0,0>, predict_intra_nv12_dc_av1_px<1,0,1>}, {predict_intra_nv12_dc_av1_px<1,1,0>, predict_intra_nv12_dc_av1_px<1,1,1>}},
        {{predict_intra_nv12_dc_av1_px<2,0,0>, predict_intra_nv12_dc_av1_px<2,0,1>}, {predict_intra_nv12_dc_av1_px<2,1,0>, predict_intra_nv12_dc_av1_px<2,1,1>}},
        {{predict_intra_nv12_dc_av1_px<3,0,0>, predict_intra_nv12_dc_av1_px<3,0,1>}, {predict_intra_nv12_dc_av1_px<3,1,0>, predict_intra_nv12_dc_av1_px<3,1,1>}},
    };
    const fptr_t fptr_avx[4][2][2] = {
        {{predict_intra_nv12_dc_av1_avx2<0,0,0>, predict_intra_nv12_dc_av1_avx2<0,0,1>}, {predict_intra_nv12_dc_av1_avx2<0,1,0>, predict_intra_nv12_dc_av1_avx2<0,1,1>}},
        {{predict_intra_nv12_dc_av1_avx2<1,0,0>, predict_intra_nv12_dc_av1_avx2<1,0,1>}, {predict_intra_nv12_dc_av1_avx2<1,1,0>, predict_intra_nv12_dc_av1_avx2<1,1,1>}},
        {{predict_intra_nv12_dc_av1_avx2<2,0,0>, predict_intra_nv12_dc_av1_avx2<2,0,1>}, {predict_intra_nv12_dc_av1_avx2<2,1,0>, predict_intra_nv12_dc_av1_avx2<2,1,1>}},
        {{predict_intra_nv12_dc_av1_avx2<3,0,0>, predict_intra_nv12_dc_av1_avx2<3,0,1>}, {predict_intra_nv12_dc_av1_avx2<3,1,0>, predict_intra_nv12_dc_av1_avx2<3,1,1>}},
    };

    for (int size = 0; size < 4; size++) {
        const int w = 4 << size;
        const int h = 4 << size;
        for (int have_left = 0; have_left < 2; have_left++) {
            for (int have_top = 0; have_top < 2; have_top++) {
                for (int ntries = 10; ntries > 0; ntries--) {
                    std::ostringstream buf;
                    buf << "Testing size=" << w << "x" << h << " have_left=" << have_left << " have_top=" << have_top;
                    SCOPED_TRACE(buf.str().c_str());
                    utils::InitRandomBuffer(top_pels, top_pels_size, 10);
                    utils::InitRandomBuffer(left_pels, left_pels_size, 10);
                    fptr_ref[size][have_left][have_top](top_pels + 32, left_pels + 32, dst_ref, pitch, 0, 0, 0);
                    fptr_avx[size][have_left][have_top](top_pels + 32, left_pels + 32, dst_avx, pitch, 0, 0, 0);
                    ASSERT_TRUE(utils::BlocksAreEqual(dst_ref, pitch, dst_avx, pitch, 2 * w, h));
                }
                if (printTicks) {
                    const uint64_t tref = utils::GetMinTicks(2000000 >> size, fptr_ref[size][have_left][have_top], top_pels + 32, left_pels + 32, dst_ref, pitch, 0, 0, 0);
                    const uint64_t tavx = utils::GetMinTicks(2000000 >> size, fptr_avx[size][have_left][have_top], top_pels + 32, left_pels + 32, dst_avx, pitch, 0, 0, 0);
                    printf("%2dx%-2d have_left=%d have_top=%d ref=%-4lld avx=%-4lld\n", w, h, have_left, have_top, tref, tavx);
                }
            }
        }
    }
}


TEST(optimization, predict_intra_av1)
{
    const int MAX_WIDTH = 32;
    const int MAX_HEIGHT = 32;
    const int top_pels_size = 2 * MAX_WIDTH + utils::AlignAvx2;
    const int left_pels_size = 2 * MAX_HEIGHT + utils::AlignAvx2;
    const int pitch = 64;
    const int dst_size = MAX_HEIGHT * pitch;

    auto top_pels_ptr = utils::MakeAlignedPtr<uint8_t>(top_pels_size, utils::AlignAvx2);
    auto left_pels_ptr = utils::MakeAlignedPtr<uint8_t>(left_pels_size, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<uint8_t>(dst_size, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<uint8_t>(dst_size, utils::AlignAvx2);
    auto top_pels = top_pels_ptr.get();
    auto left_pels = left_pels_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    srand(0x1234);

    using AV1PP::predict_intra_av1_px;
    using AV1PP::predict_intra_av1_avx2;
    typedef void(*fptr_t)(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int);
    const fptr_t fptr_ref[4][12] = {
        { predict_intra_av1_px<0,1>, predict_intra_av1_px<0,2>, predict_intra_av1_px<0,3>, predict_intra_av1_px<0,4>,
            predict_intra_av1_px<0,5>, predict_intra_av1_px<0,6>, predict_intra_av1_px<0,7>, predict_intra_av1_px<0,8>,
            predict_intra_av1_px<0,9>, predict_intra_av1_px<0,10>, predict_intra_av1_px<0,11>, predict_intra_av1_px<0,12>
        },
        { predict_intra_av1_px<1,1>, predict_intra_av1_px<1,2>, predict_intra_av1_px<1,3>, predict_intra_av1_px<1,4>,
            predict_intra_av1_px<1,5>, predict_intra_av1_px<1,6>, predict_intra_av1_px<1,7>, predict_intra_av1_px<1,8>,
            predict_intra_av1_px<1,9>, predict_intra_av1_px<1,10>, predict_intra_av1_px<1,11>, predict_intra_av1_px<1,12>
        },
        { predict_intra_av1_px<2,1>, predict_intra_av1_px<2,2>, predict_intra_av1_px<2,3>, predict_intra_av1_px<2,4>,
            predict_intra_av1_px<2,5>, predict_intra_av1_px<2,6>, predict_intra_av1_px<2,7>, predict_intra_av1_px<2,8>,
            predict_intra_av1_px<2,9>, predict_intra_av1_px<2,10>, predict_intra_av1_px<2,11>, predict_intra_av1_px<2,12>
        },
        { predict_intra_av1_px<3,1>, predict_intra_av1_px<3,2>, predict_intra_av1_px<3,3>, predict_intra_av1_px<3,4>,
            predict_intra_av1_px<3,5>, predict_intra_av1_px<3,6>, predict_intra_av1_px<3,7>, predict_intra_av1_px<3,8>,
            predict_intra_av1_px<3,9>, predict_intra_av1_px<3,10>, predict_intra_av1_px<3,11>, predict_intra_av1_px<3,12>
        },
    };
    const fptr_t fptr_avx[4][12] = {
        { predict_intra_av1_avx2<0,1>, predict_intra_av1_avx2<0,2>, predict_intra_av1_avx2<0,3>, predict_intra_av1_avx2<0,4>,
            predict_intra_av1_avx2<0,5>, predict_intra_av1_avx2<0,6>, predict_intra_av1_avx2<0,7>, predict_intra_av1_avx2<0,8>,
            predict_intra_av1_avx2<0,9>, predict_intra_av1_avx2<0,10>, predict_intra_av1_avx2<0,11>, predict_intra_av1_avx2<0,12>
        },
        { predict_intra_av1_avx2<1,1>, predict_intra_av1_avx2<1,2>, predict_intra_av1_avx2<1,3>, predict_intra_av1_avx2<1,4>,
            predict_intra_av1_avx2<1,5>, predict_intra_av1_avx2<1,6>, predict_intra_av1_avx2<1,7>, predict_intra_av1_avx2<1,8>,
            predict_intra_av1_avx2<1,9>, predict_intra_av1_avx2<1,10>, predict_intra_av1_avx2<1,11>, predict_intra_av1_avx2<1,12>
        },
        { predict_intra_av1_avx2<2,1>, predict_intra_av1_avx2<2,2>, predict_intra_av1_avx2<2,3>, predict_intra_av1_avx2<2,4>,
            predict_intra_av1_avx2<2,5>, predict_intra_av1_avx2<2,6>, predict_intra_av1_avx2<2,7>, predict_intra_av1_avx2<2,8>,
            predict_intra_av1_avx2<2,9>, predict_intra_av1_avx2<2,10>, predict_intra_av1_avx2<2,11>, predict_intra_av1_avx2<2,12>
        },
        { predict_intra_av1_avx2<3,1>, predict_intra_av1_avx2<3,2>, predict_intra_av1_avx2<3,3>, predict_intra_av1_avx2<3,4>,
            predict_intra_av1_avx2<3,5>, predict_intra_av1_avx2<3,6>, predict_intra_av1_avx2<3,7>, predict_intra_av1_avx2<3,8>,
            predict_intra_av1_avx2<3,9>, predict_intra_av1_avx2<3,10>, predict_intra_av1_avx2<3,11>, predict_intra_av1_avx2<3,12>
        },
    };

    const char *mode2str[] = {
        "DC_PRED", "V_PRED", "H_PRED", "D45_PRED", "D135_PRED", "D117_PRED", "D153_PRED",
        "D207_PRED", "D63_PRED", "SMOOTH_PRED", "SMOOTH_V_PRED", "SMOOTH_H_PRED", "PAETH_PRED"
    };

    for (int size = 0; size < 4; size++) {
        const int w = 4 << size;
        const int h = 4 << size;
        for (int mode = AV1Enc::V_PRED; mode < AV1Enc::AV1_INTRA_MODES; mode++) {
            const int max_delta = (mode >= AV1Enc::V_PRED && mode <= AV1Enc::D63_PRED) ? 3 : 0;
            for (int delta = -max_delta; delta <= max_delta; delta++) {
                for (int ntries = 10; ntries > 0; ntries--) {
                    std::ostringstream buf;
                    buf << "Testing size=" << w << "x" << h << " mode=" << mode2str[mode] << " delta=" << delta;
                    SCOPED_TRACE(buf.str().c_str());
                    utils::InitRandomBuffer(top_pels, top_pels_size, 8);
                    utils::InitRandomBuffer(left_pels, left_pels_size, 8);
                    fptr_ref[size][mode - 1](top_pels + 32, left_pels + 32, dst_ref, pitch, delta, 0, 0);
                    fptr_avx[size][mode - 1](top_pels + 32, left_pels + 32, dst_avx, pitch, delta, 0, 0);
                    ASSERT_TRUE(utils::BlocksAreEqual(dst_ref, pitch, dst_avx, pitch, w, h));
                }
                if (printTicks) {
                    const uint64_t tref = utils::GetMinTicks(2000000 >> size, fptr_ref[size][mode - 1], top_pels + 32, left_pels + 32, dst_ref, pitch, delta, 0, 0);
                    const uint64_t tavx = utils::GetMinTicks(2000000 >> size, fptr_avx[size][mode - 1], top_pels + 32, left_pels + 32, dst_avx, pitch, delta, 0, 0);
                    printf("%2dx%-2d %13s delta=%+d ref=%-4lld avx=%-4lld\n", w, h, mode2str[mode], delta, tref, tavx);
                }
            }
        }
    }
}


TEST(optimization, predict_intra_av1_hbd)
{
    const int MAX_WIDTH = 32;
    const int MAX_HEIGHT = 32;
    const int top_pels_size = 2 * MAX_WIDTH + utils::AlignAvx2;
    const int left_pels_size = 2 * MAX_HEIGHT + utils::AlignAvx2;
    const int pitch = 64;
    const int dst_size = MAX_HEIGHT * pitch;

    auto top_pels_ptr = utils::MakeAlignedPtr<uint16_t>(top_pels_size, utils::AlignAvx2);
    auto left_pels_ptr = utils::MakeAlignedPtr<uint16_t>(left_pels_size, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<uint16_t>(dst_size, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<uint16_t>(dst_size, utils::AlignAvx2);
    auto top_pels = top_pels_ptr.get();
    auto left_pels = left_pels_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    srand(0x1234);

    using AV1PP::predict_intra_av1_px;
    using AV1PP::predict_intra_av1_avx2;
    typedef void(*fptr_t)(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int);
    const fptr_t fptr_ref[4][12] = {
        { predict_intra_av1_px<0,1>, predict_intra_av1_px<0,2>, predict_intra_av1_px<0,3>, predict_intra_av1_px<0,4>,
            predict_intra_av1_px<0,5>, predict_intra_av1_px<0,6>, predict_intra_av1_px<0,7>, predict_intra_av1_px<0,8>,
            predict_intra_av1_px<0,9>, predict_intra_av1_px<0,10>, predict_intra_av1_px<0,11>, predict_intra_av1_px<0,12>
        },
        { predict_intra_av1_px<1,1>, predict_intra_av1_px<1,2>, predict_intra_av1_px<1,3>, predict_intra_av1_px<1,4>,
            predict_intra_av1_px<1,5>, predict_intra_av1_px<1,6>, predict_intra_av1_px<1,7>, predict_intra_av1_px<1,8>,
            predict_intra_av1_px<1,9>, predict_intra_av1_px<1,10>, predict_intra_av1_px<1,11>, predict_intra_av1_px<1,12>
        },
        { predict_intra_av1_px<2,1>, predict_intra_av1_px<2,2>, predict_intra_av1_px<2,3>, predict_intra_av1_px<2,4>,
            predict_intra_av1_px<2,5>, predict_intra_av1_px<2,6>, predict_intra_av1_px<2,7>, predict_intra_av1_px<2,8>,
            predict_intra_av1_px<2,9>, predict_intra_av1_px<2,10>, predict_intra_av1_px<2,11>, predict_intra_av1_px<2,12>
        },
        { predict_intra_av1_px<3,1>, predict_intra_av1_px<3,2>, predict_intra_av1_px<3,3>, predict_intra_av1_px<3,4>,
            predict_intra_av1_px<3,5>, predict_intra_av1_px<3,6>, predict_intra_av1_px<3,7>, predict_intra_av1_px<3,8>,
            predict_intra_av1_px<3,9>, predict_intra_av1_px<3,10>, predict_intra_av1_px<3,11>, predict_intra_av1_px<3,12>
        },
    };
    const fptr_t fptr_avx[4][12] = {
        { predict_intra_av1_avx2<0,1>, predict_intra_av1_avx2<0,2>, predict_intra_av1_avx2<0,3>, predict_intra_av1_avx2<0,4>,
            predict_intra_av1_avx2<0,5>, predict_intra_av1_avx2<0,6>, predict_intra_av1_avx2<0,7>, predict_intra_av1_avx2<0,8>,
            predict_intra_av1_avx2<0,9>, predict_intra_av1_avx2<0,10>, predict_intra_av1_avx2<0,11>, predict_intra_av1_avx2<0,12>
        },
        { predict_intra_av1_avx2<1,1>, predict_intra_av1_avx2<1,2>, predict_intra_av1_avx2<1,3>, predict_intra_av1_avx2<1,4>,
            predict_intra_av1_avx2<1,5>, predict_intra_av1_avx2<1,6>, predict_intra_av1_avx2<1,7>, predict_intra_av1_avx2<1,8>,
            predict_intra_av1_avx2<1,9>, predict_intra_av1_avx2<1,10>, predict_intra_av1_avx2<1,11>, predict_intra_av1_avx2<1,12>
        },
        { predict_intra_av1_avx2<2,1>, predict_intra_av1_avx2<2,2>, predict_intra_av1_avx2<2,3>, predict_intra_av1_avx2<2,4>,
            predict_intra_av1_avx2<2,5>, predict_intra_av1_avx2<2,6>, predict_intra_av1_avx2<2,7>, predict_intra_av1_avx2<2,8>,
            predict_intra_av1_avx2<2,9>, predict_intra_av1_avx2<2,10>, predict_intra_av1_avx2<2,11>, predict_intra_av1_avx2<2,12>
        },
        { predict_intra_av1_avx2<3,1>, predict_intra_av1_avx2<3,2>, predict_intra_av1_avx2<3,3>, predict_intra_av1_avx2<3,4>,
            predict_intra_av1_avx2<3,5>, predict_intra_av1_avx2<3,6>, predict_intra_av1_avx2<3,7>, predict_intra_av1_avx2<3,8>,
            predict_intra_av1_avx2<3,9>, predict_intra_av1_avx2<3,10>, predict_intra_av1_avx2<3,11>, predict_intra_av1_avx2<3,12>
        },
    };

    const char *mode2str[] = {
        "DC_PRED", "V_PRED", "H_PRED", "D45_PRED", "D135_PRED", "D117_PRED", "D153_PRED",
        "D207_PRED", "D63_PRED", "SMOOTH_PRED", "SMOOTH_V_PRED", "SMOOTH_H_PRED", "PAETH_PRED"
    };

    for (int size = 0; size < 4; size++) {
        const int w = 4 << size;
        const int h = 4 << size;
        for (int mode = AV1Enc::V_PRED; mode < AV1Enc::AV1_INTRA_MODES; mode++) {
            const int max_delta = (mode >= AV1Enc::V_PRED && mode <= AV1Enc::D63_PRED) ? 3 : 0;
            for (int delta = -max_delta; delta <= max_delta; delta++) {
                for (int ntries = 10; ntries > 0; ntries--) {
                    std::ostringstream buf;
                    buf << "Testing size=" << w << "x" << h << " mode=" << mode2str[mode] << " delta=" << delta;
                    SCOPED_TRACE(buf.str().c_str());
                    utils::InitRandomBuffer(top_pels, top_pels_size, 10);
                    utils::InitRandomBuffer(left_pels, left_pels_size, 10);
                    fptr_ref[size][mode - 1](top_pels + 32, left_pels + 32, dst_ref, pitch, delta, 0, 0);
                    fptr_avx[size][mode - 1](top_pels + 32, left_pels + 32, dst_avx, pitch, delta, 0, 0);
                    ASSERT_TRUE(utils::BlocksAreEqual(dst_ref, pitch, dst_avx, pitch, w, h));
                }
                if (printTicks) {
                    const uint64_t tref = utils::GetMinTicks(2000000 >> size, fptr_ref[size][mode - 1], top_pels + 32, left_pels + 32, dst_ref, pitch, delta, 0, 0);
                    const uint64_t tavx = utils::GetMinTicks(2000000 >> size, fptr_avx[size][mode - 1], top_pels + 32, left_pels + 32, dst_avx, pitch, delta, 0, 0);
                    printf("%2dx%-2d %13s delta=%+d ref=%-4lld avx=%-4lld\n", w, h, mode2str[mode], delta, tref, tavx);
                }
            }
        }
    }
}


TEST(optimization, predict_intra_nv12_av1)
{
    const int MAX_WIDTH = 32;
    const int MAX_HEIGHT = 32;
    const int top_pels_size = 2 * 2 * MAX_WIDTH + utils::AlignAvx2;
    const int left_pels_size = 2 * 2 * MAX_HEIGHT + utils::AlignAvx2;
    const int pitch = 128;
    const int dst_size = MAX_HEIGHT * pitch;

    auto top_pels_ptr = utils::MakeAlignedPtr<uint8_t>(top_pels_size, utils::AlignAvx2);
    auto left_pels_ptr = utils::MakeAlignedPtr<uint8_t>(left_pels_size, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<uint8_t>(dst_size, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<uint8_t>(dst_size, utils::AlignAvx2);
    auto top_pels = top_pels_ptr.get();
    auto left_pels = left_pels_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    srand(0x1234);

    using AV1PP::predict_intra_nv12_av1_px;
    using AV1PP::predict_intra_nv12_av1_avx2;
    typedef void(*fptr_t)(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int);
    const fptr_t fptr_ref[4][12] = {
        { predict_intra_nv12_av1_px<0,1>, predict_intra_nv12_av1_px<0,2>, predict_intra_nv12_av1_px<0,3>, predict_intra_nv12_av1_px<0,4>,
            predict_intra_nv12_av1_px<0,5>, predict_intra_nv12_av1_px<0,6>, predict_intra_nv12_av1_px<0,7>, predict_intra_nv12_av1_px<0,8>,
            predict_intra_nv12_av1_px<0,9>, predict_intra_nv12_av1_px<0,10>, predict_intra_nv12_av1_px<0,11>, predict_intra_nv12_av1_px<0,12>
        },
        { predict_intra_nv12_av1_px<1,1>, predict_intra_nv12_av1_px<1,2>, predict_intra_nv12_av1_px<1,3>, predict_intra_nv12_av1_px<1,4>,
            predict_intra_nv12_av1_px<1,5>, predict_intra_nv12_av1_px<1,6>, predict_intra_nv12_av1_px<1,7>, predict_intra_nv12_av1_px<1,8>,
            predict_intra_nv12_av1_px<1,9>, predict_intra_nv12_av1_px<1,10>, predict_intra_nv12_av1_px<1,11>, predict_intra_nv12_av1_px<1,12>
        },
        { predict_intra_nv12_av1_px<2,1>, predict_intra_nv12_av1_px<2,2>, predict_intra_nv12_av1_px<2,3>, predict_intra_nv12_av1_px<2,4>,
            predict_intra_nv12_av1_px<2,5>, predict_intra_nv12_av1_px<2,6>, predict_intra_nv12_av1_px<2,7>, predict_intra_nv12_av1_px<2,8>,
            predict_intra_nv12_av1_px<2,9>, predict_intra_nv12_av1_px<2,10>, predict_intra_nv12_av1_px<2,11>, predict_intra_nv12_av1_px<2,12>
        },
        { predict_intra_nv12_av1_px<3,1>, predict_intra_nv12_av1_px<3,2>, predict_intra_nv12_av1_px<3,3>, predict_intra_nv12_av1_px<3,4>,
            predict_intra_nv12_av1_px<3,5>, predict_intra_nv12_av1_px<3,6>, predict_intra_nv12_av1_px<3,7>, predict_intra_nv12_av1_px<3,8>,
            predict_intra_nv12_av1_px<3,9>, predict_intra_nv12_av1_px<3,10>, predict_intra_nv12_av1_px<3,11>, predict_intra_nv12_av1_px<3,12>
        },
    };
    const fptr_t fptr_avx[4][12] = {
        { predict_intra_nv12_av1_avx2<0,1>, predict_intra_nv12_av1_avx2<0,2>, predict_intra_nv12_av1_avx2<0,3>, predict_intra_nv12_av1_avx2<0,4>,
            predict_intra_nv12_av1_avx2<0,5>, predict_intra_nv12_av1_avx2<0,6>, predict_intra_nv12_av1_avx2<0,7>, predict_intra_nv12_av1_avx2<0,8>,
            predict_intra_nv12_av1_avx2<0,9>, predict_intra_nv12_av1_avx2<0,10>, predict_intra_nv12_av1_avx2<0,11>, predict_intra_nv12_av1_avx2<0,12>
        },
        { predict_intra_nv12_av1_avx2<1,1>, predict_intra_nv12_av1_avx2<1,2>, predict_intra_nv12_av1_avx2<1,3>, predict_intra_nv12_av1_avx2<1,4>,
            predict_intra_nv12_av1_avx2<1,5>, predict_intra_nv12_av1_avx2<1,6>, predict_intra_nv12_av1_avx2<1,7>, predict_intra_nv12_av1_avx2<1,8>,
            predict_intra_nv12_av1_avx2<1,9>, predict_intra_nv12_av1_avx2<1,10>, predict_intra_nv12_av1_avx2<1,11>, predict_intra_nv12_av1_avx2<1,12>
        },
        { predict_intra_nv12_av1_avx2<2,1>, predict_intra_nv12_av1_avx2<2,2>, predict_intra_nv12_av1_avx2<2,3>, predict_intra_nv12_av1_avx2<2,4>,
            predict_intra_nv12_av1_avx2<2,5>, predict_intra_nv12_av1_avx2<2,6>, predict_intra_nv12_av1_avx2<2,7>, predict_intra_nv12_av1_avx2<2,8>,
            predict_intra_nv12_av1_avx2<2,9>, predict_intra_nv12_av1_avx2<2,10>, predict_intra_nv12_av1_avx2<2,11>, predict_intra_nv12_av1_avx2<2,12>
        },
        { predict_intra_nv12_av1_avx2<3,1>, predict_intra_nv12_av1_avx2<3,2>, predict_intra_nv12_av1_avx2<3,3>, predict_intra_nv12_av1_avx2<3,4>,
            predict_intra_nv12_av1_avx2<3,5>, predict_intra_nv12_av1_avx2<3,6>, predict_intra_nv12_av1_avx2<3,7>, predict_intra_nv12_av1_avx2<3,8>,
            predict_intra_nv12_av1_avx2<3,9>, predict_intra_nv12_av1_avx2<3,10>, predict_intra_nv12_av1_avx2<3,11>, predict_intra_nv12_av1_avx2<3,12>
        },
    };

    const char *mode2str[] = {
        "DC_PRED", "V_PRED", "H_PRED", "D45_PRED", "D135_PRED", "D117_PRED", "D153_PRED",
        "D207_PRED", "D63_PRED", "SMOOTH_PRED", "SMOOTH_V_PRED", "SMOOTH_H_PRED", "PAETH_PRED"
    };

    for (int size = 0; size < 4; size++) {
        const int w = 4 << size;
        const int h = 4 << size;
        for (int mode = AV1Enc::V_PRED; mode < AV1Enc::AV1_INTRA_MODES; mode++) {
            const int max_delta = (mode >= AV1Enc::V_PRED && mode <= AV1Enc::D63_PRED) ? 3 : 0;
            for (int delta = -max_delta; delta <= max_delta; delta++) {
                for (int ntries = 10; ntries > 0; ntries--) {
                    std::ostringstream buf;
                    buf << "Testing size=" << w << "x" << h << " mode=" << mode2str[mode] << " delta=" << delta;
                    SCOPED_TRACE(buf.str().c_str());
                    utils::InitRandomBuffer(top_pels, top_pels_size, 8);
                    utils::InitRandomBuffer(left_pels, left_pels_size, 8);
                    fptr_ref[size][mode - 1](top_pels + 32, left_pels + 32, dst_ref, pitch, delta, 0, 0);
                    fptr_avx[size][mode - 1](top_pels + 32, left_pels + 32, dst_avx, pitch, delta, 0, 0);
                    ASSERT_TRUE(utils::BlocksAreEqual(dst_ref, pitch, dst_avx, pitch, 2 * w, h));
                }
                if (printTicks && delta == 0) {
                    const uint64_t tref = utils::GetMinTicks(1000000 >> size, fptr_ref[size][mode - 1], top_pels + 32, left_pels + 32, dst_ref, pitch, delta, 0, 0);
                    const uint64_t tavx = utils::GetMinTicks(1000000 >> size, fptr_avx[size][mode - 1], top_pels + 32, left_pels + 32, dst_avx, pitch, delta, 0, 0);
                    printf("%2dx%-2d %13s delta=%+d ref=%-4lld avx=%-4lld\n", w, h, mode2str[mode], delta, tref, tavx);
                }
            }
        }
    }
}


TEST(optimization, predict_intra_nv12_av1_hbd)
{
    const int MAX_WIDTH = 32;
    const int MAX_HEIGHT = 32;
    const int top_pels_size = 2 * 2 * MAX_WIDTH + utils::AlignAvx2;
    const int left_pels_size = 2 * 2 * MAX_HEIGHT + utils::AlignAvx2;
    const int pitch = 128;
    const int dst_size = MAX_HEIGHT * pitch;

    auto top_pels_ptr = utils::MakeAlignedPtr<uint16_t>(top_pels_size, utils::AlignAvx2);
    auto left_pels_ptr = utils::MakeAlignedPtr<uint16_t>(left_pels_size, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<uint16_t>(dst_size, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<uint16_t>(dst_size, utils::AlignAvx2);
    auto top_pels = top_pels_ptr.get();
    auto left_pels = left_pels_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    srand(0x1234);

    using AV1PP::predict_intra_nv12_av1_px;
    using AV1PP::predict_intra_nv12_av1_avx2;
    typedef void(*fptr_t)(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int);
    const fptr_t fptr_ref[4][12] = {
        { predict_intra_nv12_av1_px<0,1>, predict_intra_nv12_av1_px<0,2>, predict_intra_nv12_av1_px<0,3>, predict_intra_nv12_av1_px<0,4>,
            predict_intra_nv12_av1_px<0,5>, predict_intra_nv12_av1_px<0,6>, predict_intra_nv12_av1_px<0,7>, predict_intra_nv12_av1_px<0,8>,
            predict_intra_nv12_av1_px<0,9>, predict_intra_nv12_av1_px<0,10>, predict_intra_nv12_av1_px<0,11>, predict_intra_nv12_av1_px<0,12>
        },
        { predict_intra_nv12_av1_px<1,1>, predict_intra_nv12_av1_px<1,2>, predict_intra_nv12_av1_px<1,3>, predict_intra_nv12_av1_px<1,4>,
            predict_intra_nv12_av1_px<1,5>, predict_intra_nv12_av1_px<1,6>, predict_intra_nv12_av1_px<1,7>, predict_intra_nv12_av1_px<1,8>,
            predict_intra_nv12_av1_px<1,9>, predict_intra_nv12_av1_px<1,10>, predict_intra_nv12_av1_px<1,11>, predict_intra_nv12_av1_px<1,12>
        },
        { predict_intra_nv12_av1_px<2,1>, predict_intra_nv12_av1_px<2,2>, predict_intra_nv12_av1_px<2,3>, predict_intra_nv12_av1_px<2,4>,
            predict_intra_nv12_av1_px<2,5>, predict_intra_nv12_av1_px<2,6>, predict_intra_nv12_av1_px<2,7>, predict_intra_nv12_av1_px<2,8>,
            predict_intra_nv12_av1_px<2,9>, predict_intra_nv12_av1_px<2,10>, predict_intra_nv12_av1_px<2,11>, predict_intra_nv12_av1_px<2,12>
        },
        { predict_intra_nv12_av1_px<3,1>, predict_intra_nv12_av1_px<3,2>, predict_intra_nv12_av1_px<3,3>, predict_intra_nv12_av1_px<3,4>,
            predict_intra_nv12_av1_px<3,5>, predict_intra_nv12_av1_px<3,6>, predict_intra_nv12_av1_px<3,7>, predict_intra_nv12_av1_px<3,8>,
            predict_intra_nv12_av1_px<3,9>, predict_intra_nv12_av1_px<3,10>, predict_intra_nv12_av1_px<3,11>, predict_intra_nv12_av1_px<3,12>
        },
    };
    const fptr_t fptr_avx[4][12] = {
        { predict_intra_nv12_av1_avx2<0,1>, predict_intra_nv12_av1_avx2<0,2>, predict_intra_nv12_av1_avx2<0,3>, predict_intra_nv12_av1_avx2<0,4>,
            predict_intra_nv12_av1_avx2<0,5>, predict_intra_nv12_av1_avx2<0,6>, predict_intra_nv12_av1_avx2<0,7>, predict_intra_nv12_av1_avx2<0,8>,
            predict_intra_nv12_av1_avx2<0,9>, predict_intra_nv12_av1_avx2<0,10>, predict_intra_nv12_av1_avx2<0,11>, predict_intra_nv12_av1_avx2<0,12>
        },
        { predict_intra_nv12_av1_avx2<1,1>, predict_intra_nv12_av1_avx2<1,2>, predict_intra_nv12_av1_avx2<1,3>, predict_intra_nv12_av1_avx2<1,4>,
            predict_intra_nv12_av1_avx2<1,5>, predict_intra_nv12_av1_avx2<1,6>, predict_intra_nv12_av1_avx2<1,7>, predict_intra_nv12_av1_avx2<1,8>,
            predict_intra_nv12_av1_avx2<1,9>, predict_intra_nv12_av1_avx2<1,10>, predict_intra_nv12_av1_avx2<1,11>, predict_intra_nv12_av1_avx2<1,12>
        },
        { predict_intra_nv12_av1_avx2<2,1>, predict_intra_nv12_av1_avx2<2,2>, predict_intra_nv12_av1_avx2<2,3>, predict_intra_nv12_av1_avx2<2,4>,
            predict_intra_nv12_av1_avx2<2,5>, predict_intra_nv12_av1_avx2<2,6>, predict_intra_nv12_av1_avx2<2,7>, predict_intra_nv12_av1_avx2<2,8>,
            predict_intra_nv12_av1_avx2<2,9>, predict_intra_nv12_av1_avx2<2,10>, predict_intra_nv12_av1_avx2<2,11>, predict_intra_nv12_av1_avx2<2,12>
        },
        { predict_intra_nv12_av1_avx2<3,1>, predict_intra_nv12_av1_avx2<3,2>, predict_intra_nv12_av1_avx2<3,3>, predict_intra_nv12_av1_avx2<3,4>,
            predict_intra_nv12_av1_avx2<3,5>, predict_intra_nv12_av1_avx2<3,6>, predict_intra_nv12_av1_avx2<3,7>, predict_intra_nv12_av1_avx2<3,8>,
            predict_intra_nv12_av1_avx2<3,9>, predict_intra_nv12_av1_avx2<3,10>, predict_intra_nv12_av1_avx2<3,11>, predict_intra_nv12_av1_avx2<3,12>
        },
    };

    const char *mode2str[] = {
        "DC_PRED", "V_PRED", "H_PRED", "D45_PRED", "D135_PRED", "D117_PRED", "D153_PRED",
        "D207_PRED", "D63_PRED", "SMOOTH_PRED", "SMOOTH_V_PRED", "SMOOTH_H_PRED", "PAETH_PRED"
    };

    for (int size = 0; size < 4; size++) {
        const int w = 4 << size;
        const int h = 4 << size;
        for (int mode = AV1Enc::V_PRED; mode < AV1Enc::AV1_INTRA_MODES; mode++) {
            const int max_delta = (mode >= AV1Enc::V_PRED && mode <= AV1Enc::D63_PRED) ? 3 : 0;
            for (int delta = -max_delta; delta <= max_delta; delta++) {
                for (int ntries = 10; ntries > 0; ntries--) {
                    std::ostringstream buf;
                    buf << "Testing size=" << w << "x" << h << " mode=" << mode2str[mode] << " delta=" << delta;
                    SCOPED_TRACE(buf.str().c_str());
                    utils::InitRandomBuffer(top_pels, top_pels_size, 10);
                    utils::InitRandomBuffer(left_pels, left_pels_size, 10);
                    fptr_ref[size][mode - 1](top_pels + 32, left_pels + 32, dst_ref, pitch, delta, 0, 0);
                    fptr_avx[size][mode - 1](top_pels + 32, left_pels + 32, dst_avx, pitch, delta, 0, 0);
                    ASSERT_TRUE(utils::BlocksAreEqual(dst_ref, pitch, dst_avx, pitch, 2 * w, h));
                }
                if (printTicks && delta == 0) {
                    const uint64_t tref = utils::GetMinTicks(1000000 >> size, fptr_ref[size][mode - 1], top_pels + 32, left_pels + 32, dst_ref, pitch, delta, 0, 0);
                    const uint64_t tavx = utils::GetMinTicks(1000000 >> size, fptr_avx[size][mode - 1], top_pels + 32, left_pels + 32, dst_avx, pitch, delta, 0, 0);
                    printf("%2dx%-2d %13s delta=%+d ref=%-4lld avx=%-4lld\n", w, h, mode2str[mode], delta, tref, tavx);
                }
            }
        }
    }
}


TEST(optimization, average)
{
    const int pitch = 96;
    const int srcSize = pitch*pitch;
    const int dstSize = pitch*pitch;

    auto src1_ptr = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
    auto src2_ptr = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
    auto dst_ref_ptr = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
    auto dst_avx_ptr = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
    auto src1 = src1_ptr.get();
    auto src2 = src2_ptr.get();
    auto dst_ref = dst_ref_ptr.get();
    auto dst_avx = dst_avx_ptr.get();

    srand(0x1234);

    utils::InitRandomBlock(src1, pitch, pitch, pitch, 8);
    utils::InitRandomBlock(src2, pitch, pitch, pitch, 8);
    utils::InitRandomBlock(dst_ref, pitch, pitch, pitch, 8);
    memcpy(dst_avx, dst_ref, sizeof(*dst_ref) * dstSize);

    Ipp32s dims[][2] = {
        {4,4}, {4,8}, {8,4}, {8,8}, {8,16}, {16,8}, {16,16}, {16,32}, {32,16}, {32,32}, {32,64}, {64,32}, {64,64}
    };

    utils::InitRandomBlock(src1, pitch, pitch, pitch, 8);
    utils::InitRandomBlock(src2, pitch, pitch, pitch, 8);
    utils::InitRandomBlock(dst_ref, pitch, pitch, pitch, 8);
    memcpy(dst_avx, dst_ref, sizeof(*dst_ref) * dstSize);
    for (int h = 4; h <= 8; h <<= 1) {
        const int w = 4;
        AV1PP::average_px<w>  (src1, pitch, src2, pitch, dst_ref, pitch, h);
        AV1PP::average_avx2<w>(src1, pitch, src2, pitch, dst_avx, pitch, h);
        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
    }
    for (int h = 4; h <= 16; h <<= 1) {
        const int w = 8;
        AV1PP::average_px<w>  (src1, pitch, src2, pitch, dst_ref, pitch, h);
        AV1PP::average_avx2<w>(src1, pitch, src2, pitch, dst_avx, pitch, h);
        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
    }
    for (int h = 8; h <= 32; h <<= 1) {
        const int w = 16;
        AV1PP::average_px<w>  (src1, pitch, src2, pitch, dst_ref, pitch, h);
        AV1PP::average_avx2<w>(src1, pitch, src2, pitch, dst_avx, pitch, h);
        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
    }
    for (int h = 16; h <= 64; h <<= 1) {
        const int w = 32;
        AV1PP::average_px<w>  (src1, pitch, src2, pitch, dst_ref, pitch, h);
        AV1PP::average_avx2<w>(src1, pitch, src2, pitch, dst_avx, pitch, h);
        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
    }
    for (int h = 32; h <= 64; h <<= 1) {
        const int w = 64;
        AV1PP::average_px<w>  (src1, pitch, src2, pitch, dst_ref, pitch, h);
        AV1PP::average_avx2<w>(src1, pitch, src2, pitch, dst_avx, pitch, h);
        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
    }

    if (printTicks) {
        Ipp64u tref, tavx;
        tref = utils::GetMinTicks(10000,  AV1PP::average_px<4>,   src1, pitch, src2, pitch, dst_ref, pitch, 4);
        tavx = utils::GetMinTicks(100000, AV1PP::average_avx2<4>, src1, pitch, src2, pitch, dst_ref, pitch, 4);
        printf(" 4x4  ref=%-6lld avx2=%-4lld\n", tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::average_px<8>,   src1, pitch, src2, pitch, dst_ref, pitch, 8);
        tavx = utils::GetMinTicks(100000, AV1PP::average_avx2<8>, src1, pitch, src2, pitch, dst_ref, pitch, 8);
        printf(" 8x8  ref=%-6lld avx2=%-4lld\n", tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::average_px<16>,   src1, pitch, src2, pitch, dst_ref, pitch, 16);
        tavx = utils::GetMinTicks(100000, AV1PP::average_avx2<16>, src1, pitch, src2, pitch, dst_ref, pitch, 16);
        printf("16x16 ref=%-6lld avx2=%-4lld\n", tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::average_px<32>,   src1, pitch, src2, pitch, dst_ref, pitch, 32);
        tavx = utils::GetMinTicks(100000, AV1PP::average_avx2<32>, src1, pitch, src2, pitch, dst_ref, pitch, 32);
        printf("32x32 ref=%-6lld avx2=%-4lld\n", tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::average_px<64>,   src1, pitch, src2, pitch, dst_ref, pitch, 64);
        tavx = utils::GetMinTicks(100000, AV1PP::average_avx2<64>, src1, pitch, src2, pitch, dst_ref, pitch, 64);
        printf("64x64 ref=%-6lld avx2=%-4lld\n", tref, tavx);
    }
}

alignas(256) static const short bilinear_filters[16][8] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },  { 0, 0, 0, 120, 8, 0, 0, 0 },
    { 0, 0, 0, 112, 16, 0, 0, 0 }, { 0, 0, 0, 104, 24, 0, 0, 0 },
    { 0, 0, 0, 96, 32, 0, 0, 0 },  { 0, 0, 0, 88, 40, 0, 0, 0 },
    { 0, 0, 0, 80, 48, 0, 0, 0 },  { 0, 0, 0, 72, 56, 0, 0, 0 },
    { 0, 0, 0, 64, 64, 0, 0, 0 },  { 0, 0, 0, 56, 72, 0, 0, 0 },
    { 0, 0, 0, 48, 80, 0, 0, 0 },  { 0, 0, 0, 40, 88, 0, 0, 0 },
    { 0, 0, 0, 32, 96, 0, 0, 0 },  { 0, 0, 0, 24, 104, 0, 0, 0 },
    { 0, 0, 0, 16, 112, 0, 0, 0 }, { 0, 0, 0, 8, 120, 0, 0, 0 }
};
alignas(256) static const short sub_pel_filters_8[16][8] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },        { 0, 1, -5, 126, 8, -3, 1, 0 },
    { -1, 3, -10, 122, 18, -6, 2, 0 },   { -1, 4, -13, 118, 27, -9, 3, -1 },
    { -1, 4, -16, 112, 37, -11, 4, -1 }, { -1, 5, -18, 105, 48, -14, 4, -1 },
    { -1, 5, -19, 97, 58, -16, 5, -1 },  { -1, 6, -19, 88, 68, -18, 5, -1 },
    { -1, 6, -19, 78, 78, -19, 6, -1 },  { -1, 5, -18, 68, 88, -19, 6, -1 },
    { -1, 5, -16, 58, 97, -19, 5, -1 },  { -1, 4, -14, 48, 105, -18, 5, -1 },
    { -1, 4, -11, 37, 112, -16, 4, -1 }, { -1, 3, -9, 27, 118, -13, 4, -1 },
    { 0, 2, -6, 18, 122, -10, 3, -1 },   { 0, 1, -3, 8, 126, -5, 1, 0 }
};
alignas(256) static const short sub_pel_filters_8s[16][8] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },         { -1, 3, -7, 127, 8, -3, 1, 0 },
    { -2, 5, -13, 125, 17, -6, 3, -1 },   { -3, 7, -17, 121, 27, -10, 5, -2 },
    { -4, 9, -20, 115, 37, -13, 6, -2 },  { -4, 10, -23, 108, 48, -16, 8, -3 },
    { -4, 10, -24, 100, 59, -19, 9, -3 }, { -4, 11, -24, 90, 70, -21, 10, -4 },
    { -4, 11, -23, 80, 80, -23, 11, -4 }, { -4, 10, -21, 70, 90, -24, 11, -4 },
    { -3, 9, -19, 59, 100, -24, 10, -4 }, { -3, 8, -16, 48, 108, -23, 10, -4 },
    { -2, 6, -13, 37, 115, -20, 9, -4 },  { -2, 5, -10, 27, 121, -17, 7, -3 },
    { -1, 3, -6, 17, 125, -13, 5, -2 },   { 0, 1, -3, 8, 127, -7, 3, -1 }
};
alignas(256) static const short sub_pel_filters_8lp[16][8] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },       { -3, -1, 32, 64, 38, 1, -3, 0 },
    { -2, -2, 29, 63, 41, 2, -3, 0 },   { -2, -2, 26, 63, 43, 4, -4, 0 },
    { -2, -3, 24, 62, 46, 5, -4, 0 },   { -2, -3, 21, 60, 49, 7, -4, 0 },
    { -1, -4, 18, 59, 51, 9, -4, 0 },   { -1, -4, 16, 57, 53, 12, -4, -1 },
    { -1, -4, 14, 55, 55, 14, -4, -1 }, { -1, -4, 12, 53, 57, 16, -4, -1 },
    { 0, -4, 9, 51, 59, 18, -4, -1 },   { 0, -4, 7, 49, 60, 21, -3, -2 },
    { 0, -4, 5, 46, 62, 24, -3, -2 },   { 0, -4, 4, 43, 63, 26, -2, -2 },
    { 0, -3, 2, 41, 63, 29, -2, -2 },   { 0, -3, 1, 38, 64, 32, -1, -3 }
};
const short (*vp9_filter_kernels[4])[8] = {
    sub_pel_filters_8, sub_pel_filters_8lp, sub_pel_filters_8s, bilinear_filters
};

namespace interp {
    const short (*vp9_filter_kernels[4])[8] = {
        sub_pel_filters_8, sub_pel_filters_8lp, sub_pel_filters_8s, bilinear_filters
    };
    template <int w, int... heights> void test() {
        const int pitch = 96;
        const int srcSize = pitch*pitch;
        const int dstSize = pitch*pitch;

        auto src_ptr = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
        auto dst_ref_ptr = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
        auto dst_sse_ptr = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
        auto dst_avx_ptr = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
        auto src_all = src_ptr.get();
        auto src = src_all + 8 * pitch + 8;
        auto dst_ref = dst_ref_ptr.get();
        auto dst_sse = dst_sse_ptr.get();
        auto dst_avx = dst_avx_ptr.get();

        int height[sizeof...(heights)] = {heights...};

        for (const int h: height) {
            utils::InitRandomBlock(src_all, pitch, pitch, pitch, 8);
            utils::InitRandomBlock(dst_ref, pitch, pitch, pitch, 8);
            memcpy(dst_sse, dst_ref, sizeof(*dst_ref) * dstSize);
            memcpy(dst_avx, dst_ref, sizeof(*dst_ref) * dstSize);

            for (int filter = 0; filter < 4; filter++) {
                for (int dx = 1; dx < 16; dx++) {
                    std::ostringstream buf;
                    buf << "Testing " << w << "x" << h << " filter=" << filter << " dx=" << dx;
                    SCOPED_TRACE(buf.str().c_str());
                    const Ipp16s *filtx = vp9_filter_kernels[filter][dx];
                    const Ipp16s *filty = vp9_filter_kernels[filter][16 - dx];
                    { SCOPED_TRACE("vert no avg");
                        AV1PP::interp_px   <w, 0, 1, 0>(src, pitch, dst_ref, pitch, NULL, filty, h);
                        AV1PP::interp_ssse3<w, 0, 1, 0>(src, pitch, dst_sse, pitch, NULL, filty, h);
                        AV1PP::interp_avx2 <w, 0, 1, 0>(src, pitch, dst_avx, pitch, NULL, filty, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_sse, sizeof(*dst_ref) * dstSize));
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("vert w/ avg");
                        AV1PP::interp_px   <w, 0, 1, 1>(src, pitch, dst_ref, pitch, NULL, filty, h);
                        AV1PP::interp_ssse3<w, 0, 1, 1>(src, pitch, dst_sse, pitch, NULL, filty, h);
                        AV1PP::interp_avx2 <w, 0, 1, 1>(src, pitch, dst_avx, pitch, NULL, filty, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_sse, sizeof(*dst_ref) * dstSize));
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz no avg");
                        AV1PP::interp_px   <w, 1, 0, 0>(src, pitch, dst_ref, pitch, filtx, NULL, h);
                        AV1PP::interp_ssse3<w, 1, 0, 0>(src, pitch, dst_sse, pitch, filtx, NULL, h);
                        AV1PP::interp_avx2 <w, 1, 0, 0>(src, pitch, dst_avx, pitch, filtx, NULL, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_sse, sizeof(*dst_ref) * dstSize));
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz w/ avg");
                        AV1PP::interp_px   <w, 1, 0, 1>(src, pitch, dst_ref, pitch, filtx, NULL, h);
                        AV1PP::interp_ssse3<w, 1, 0, 1>(src, pitch, dst_sse, pitch, filtx, NULL, h);
                        AV1PP::interp_avx2 <w, 1, 0, 1>(src, pitch, dst_avx, pitch, filtx, NULL, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_sse, sizeof(*dst_ref) * dstSize));
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both no avg");
                        AV1PP::interp_px   <w, 1, 1, 0>(src, pitch, dst_ref, pitch, filtx, filty, h);
                        AV1PP::interp_ssse3<w, 1, 1, 0>(src, pitch, dst_sse, pitch, filtx, filty, h);
                        AV1PP::interp_avx2 <w, 1, 1, 0>(src, pitch, dst_avx, pitch, filtx, filty, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_sse, sizeof(*dst_ref) * dstSize));
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both w/ avg");
                        AV1PP::interp_px   <w, 1, 1, 1>(src, pitch, dst_ref, pitch, filtx, filty, h);
                        AV1PP::interp_ssse3<w, 1, 1, 1>(src, pitch, dst_sse, pitch, filtx, filty, h);
                        AV1PP::interp_avx2 <w, 1, 1, 1>(src, pitch, dst_avx, pitch, filtx, filty, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_sse, sizeof(*dst_ref) * dstSize));
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                }
            }

            { SCOPED_TRACE("copy no avg");
                AV1PP::interp_px   <w, 0, 0, 0>(src, pitch, dst_ref, pitch, NULL, NULL, h);
                AV1PP::interp_ssse3<w, 0, 0, 0>(src, pitch, dst_sse, pitch, NULL, NULL, h);
                AV1PP::interp_avx2 <w, 0, 0, 0>(src, pitch, dst_avx, pitch, NULL, NULL, h);
                ASSERT_EQ(0, memcmp(dst_ref, dst_sse, sizeof(*dst_ref) * dstSize));
                ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
            }
            { SCOPED_TRACE("copy w/ avg");
                AV1PP::interp_px   <w, 0, 0, 1>(src, pitch, dst_ref, pitch, NULL, NULL, h);
                AV1PP::interp_ssse3<w, 0, 0, 1>(src, pitch, dst_sse, pitch, NULL, NULL, h);
                AV1PP::interp_avx2 <w, 0, 0, 1>(src, pitch, dst_avx, pitch, NULL, NULL, h);
                ASSERT_EQ(0, memcmp(dst_ref, dst_sse, sizeof(*dst_ref) * dstSize));
                ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
            }
        }
    }
    template <int w> void time() {
        const int pitch = 96;
        const int srcSize = pitch*pitch;
        const int dstSize = pitch*pitch;

        auto src_ptr = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
        auto dst_ptr = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
        auto src_all = src_ptr.get();
        auto src = src_all + 8 * pitch + 8;
        auto dst = dst_ptr.get();

        const int h = w; // time square blocks
        const Ipp16s *fx = vp9_filter_kernels[0][4];
        const Ipp16s *fy = vp9_filter_kernels[0][8];
        Ipp64u tref, tsse, tavx;
        tref = utils::GetMinTicks(10000,  AV1PP::interp_px   <w, 0, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        tsse = utils::GetMinTicks(100000, AV1PP::interp_ssse3<w, 0, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_avx2 <w, 0, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d copy no avg: ref=%-6lld ssse3=%-4lld avx2=%-4lld\n", w, h, tref, tsse, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_px   <w, 0, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        tsse = utils::GetMinTicks(100000, AV1PP::interp_ssse3<w, 0, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_avx2 <w, 0, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d copy w/ avg: ref=%-6lld ssse3=%-4lld avx2=%-4lld\n", w, h, tref, tsse, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_px   <w, 0, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        tsse = utils::GetMinTicks(100000, AV1PP::interp_ssse3<w, 0, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_avx2 <w, 0, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d vert no avg: ref=%-6lld ssse3=%-4lld avx2=%-4lld\n", w, h, tref, tsse, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_px   <w, 0, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        tsse = utils::GetMinTicks(100000, AV1PP::interp_ssse3<w, 0, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_avx2 <w, 0, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d vert w/ avg: ref=%-6lld ssse3=%-4lld avx2=%-4lld\n", w, h, tref, tsse, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_px   <w, 1, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        tsse = utils::GetMinTicks(100000, AV1PP::interp_ssse3<w, 1, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_avx2 <w, 1, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d horz no avg: ref=%-6lld ssse3=%-4lld avx2=%-4lld\n", w, h, tref, tsse, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_px   <w, 1, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        tsse = utils::GetMinTicks(100000, AV1PP::interp_ssse3<w, 1, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_avx2 <w, 1, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d horz w/ avg: ref=%-6lld ssse3=%-4lld avx2=%-4lld\n", w, h, tref, tsse, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_px   <w, 1, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        tsse = utils::GetMinTicks(100000, AV1PP::interp_ssse3<w, 1, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_avx2 <w, 1, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d both no avg: ref=%-6lld ssse3=%-4lld avx2=%-4lld\n", w, h, tref, tsse, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_px   <w, 1, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        tsse = utils::GetMinTicks(100000, AV1PP::interp_ssse3<w, 1, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_avx2 <w, 1, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d both w/ avg: ref=%-6lld ssse3=%-4lld avx2=%-4lld\n", w, h, tref, tsse, tavx);
    }
};

TEST(optimization, interp)
{
    srand(0x1234);
    interp::test< 4, 4,8>();
    interp::test< 8, 4,8,16>();
    interp::test<16, 8,16,32>();
    interp::test<32, 16,32,64>();
    interp::test<64, 32,64>();

    if (printTicks) {
        interp::time<4>();
        interp::time<8>();
        interp::time<16>();
        interp::time<32>();
        interp::time<64>();
    }
}

namespace interp_nv12 {
    template <int w, int... heights> void test() {
        const int pitch = 96;
        const int srcSize = pitch*pitch;
        const int dstSize = pitch*pitch;

        auto src_ptr = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
        auto dst_ref_ptr = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
        auto dst_avx_ptr = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
        auto src_all = src_ptr.get();
        auto src = src_all + 8 * pitch + 8;
        auto dst_ref = dst_ref_ptr.get();
        auto dst_avx = dst_avx_ptr.get();

        int height[sizeof...(heights)] = {heights...};

        for (const int h: height) {
            utils::InitRandomBlock(src_all, pitch, pitch, pitch, 8);
            utils::InitRandomBlock(dst_ref, pitch, pitch, pitch, 8);
            memcpy(dst_avx, dst_ref, sizeof(*dst_ref) * dstSize);

            for (int filter = 0; filter < 4; filter++) {
                for (int dx = 1; dx < 16; dx++) {
                    std::ostringstream buf;
                    buf << "Testing " << w << "x" << h << " filter=" << filter << " dx=" << dx;
                    SCOPED_TRACE(buf.str().c_str());
                    const Ipp16s *filtx = vp9_filter_kernels[filter][dx];
                    const Ipp16s *filty = vp9_filter_kernels[filter][16 - dx];
                    { SCOPED_TRACE("vert no avg");
                        AV1PP::interp_nv12_px   <w, 0, 1, 0>(src, pitch, dst_ref, pitch, NULL, filty, h);
                        AV1PP::interp_nv12_avx2 <w, 0, 1, 0>(src, pitch, dst_avx, pitch, NULL, filty, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("vert w/ avg");
                        AV1PP::interp_nv12_px   <w, 0, 1, 1>(src, pitch, dst_ref, pitch, NULL, filty, h);
                        AV1PP::interp_nv12_avx2 <w, 0, 1, 1>(src, pitch, dst_avx, pitch, NULL, filty, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz no avg");
                        AV1PP::interp_nv12_px   <w, 1, 0, 0>(src, pitch, dst_ref, pitch, filtx, NULL, h);
                        AV1PP::interp_nv12_avx2 <w, 1, 0, 0>(src, pitch, dst_avx, pitch, filtx, NULL, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz w/ avg");
                        AV1PP::interp_nv12_px   <w, 1, 0, 1>(src, pitch, dst_ref, pitch, filtx, NULL, h);
                        AV1PP::interp_nv12_avx2 <w, 1, 0, 1>(src, pitch, dst_avx, pitch, filtx, NULL, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both no avg");
                        AV1PP::interp_nv12_px   <w, 1, 1, 0>(src, pitch, dst_ref, pitch, filtx, filty, h);
                        AV1PP::interp_nv12_avx2 <w, 1, 1, 0>(src, pitch, dst_avx, pitch, filtx, filty, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both w/ avg");
                        AV1PP::interp_nv12_px   <w, 1, 1, 1>(src, pitch, dst_ref, pitch, filtx, filty, h);
                        AV1PP::interp_nv12_avx2 <w, 1, 1, 1>(src, pitch, dst_avx, pitch, filtx, filty, h);
                        ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
                    }
                }
            }

            { SCOPED_TRACE("copy no avg");
                AV1PP::interp_nv12_px   <w, 0, 0, 0>(src, pitch, dst_ref, pitch, NULL, NULL, h);
                AV1PP::interp_nv12_avx2 <w, 0, 0, 0>(src, pitch, dst_avx, pitch, NULL, NULL, h);
                ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
            }
            { SCOPED_TRACE("copy w/ avg");
                AV1PP::interp_nv12_px   <w, 0, 0, 1>(src, pitch, dst_ref, pitch, NULL, NULL, h);
                AV1PP::interp_nv12_avx2 <w, 0, 0, 1>(src, pitch, dst_avx, pitch, NULL, NULL, h);
                ASSERT_EQ(0, memcmp(dst_ref, dst_avx, sizeof(*dst_ref) * dstSize));
            }
        }
    }
    template <int w> void time() {
        const int pitch = 96;
        const int srcSize = pitch*pitch;
        const int dstSize = pitch*pitch;

        auto src_ptr = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
        auto dst_ptr = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
        auto src_all = src_ptr.get();
        auto src = src_all + 8 * pitch + 8;
        auto dst = dst_ptr.get();

        const int h = w; // time square blocks
        const Ipp16s *fx = vp9_filter_kernels[0][4];
        const Ipp16s *fy = vp9_filter_kernels[0][8];
        Ipp64u tref, tavx;
        tref = utils::GetMinTicks(10000,  AV1PP::interp_nv12_px   <w, 0, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_nv12_avx2 <w, 0, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d copy no avg: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_nv12_px   <w, 0, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_nv12_avx2 <w, 0, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d copy w/ avg: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_nv12_px   <w, 0, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_nv12_avx2 <w, 0, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d vert no avg: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_nv12_px   <w, 0, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_nv12_avx2 <w, 0, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d vert w/ avg: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_nv12_px   <w, 1, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_nv12_avx2 <w, 1, 0, 0>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d horz no avg: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_nv12_px   <w, 1, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_nv12_avx2 <w, 1, 0, 1>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d horz w/ avg: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_nv12_px   <w, 1, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_nv12_avx2 <w, 1, 1, 0>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d both no avg: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        tref = utils::GetMinTicks(10000,  AV1PP::interp_nv12_px   <w, 1, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        tavx = utils::GetMinTicks(100000, AV1PP::interp_nv12_avx2 <w, 1, 1, 1>, src, pitch, dst, pitch, fx, fy, h);
        printf("%2dx%-2d both w/ avg: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
    }
};

TEST(optimization, interp_nv12)
{
    srand(0x1234);
    interp_nv12::test< 4, 4,8>();
    interp_nv12::test< 8, 4,8,16>();
    interp_nv12::test<16, 8,16,32>();
    interp_nv12::test<32, 16,32>();

    if (printTicks) {
        interp_nv12::time<4>();
        interp_nv12::time<8>();
        interp_nv12::time<16>();
        interp_nv12::time<32>();
    }
}


namespace interp_av1 {
    using AV1Enc::av1_filter_kernels;

    const int sw = RoundUpPowerOf2(64 + 7, utils::AlignAvx2);
    const int sh = 64 + 7;
    const int dw = 64;
    const int dh = 64;
    const int pitchSrc = sw;
    const int pitchDst = dw;
    const int srcSize = pitchSrc * sh;
    const int dstSize = pitchDst * dh;

    template <int w, int... heights> void test() {
        auto src_ptr       = utils::MakeAlignedPtr<uint8_t>(srcSize, utils::AlignAvx2);
        auto ref0_ptr      = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);
        auto dst8_ref_ptr  = utils::MakeAlignedPtr<uint8_t>(dstSize, utils::AlignAvx2);
        auto dst8_avx_ptr  = utils::MakeAlignedPtr<uint8_t>(dstSize, utils::AlignAvx2);
        auto dst16_ref_ptr = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);
        auto dst16_avx_ptr = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);

        auto src_padded = src_ptr.get();
        auto src        = src_padded + 3 * pitchSrc + RoundUpPowerOf2(3, utils::AlignAvx2);
        auto ref0       = ref0_ptr.get();
        auto dst8_ref   = dst8_ref_ptr.get();
        auto dst8_avx   = dst8_avx_ptr.get();
        auto dst16_ref  = dst16_ref_ptr.get();
        auto dst16_avx  = dst16_avx_ptr.get();

        int height[sizeof...(heights)] = {heights...};

        for (const int h: height) {
            utils::InitRandomBuffer(src_padded, srcSize, 8);
            utils::InitRandomDiffBuffer(ref0, dstSize, 12);
            utils::InitRandomBuffer(dst8_ref, dstSize, 8);
            utils::InitRandomDiffBuffer(dst16_ref, dstSize, 15);
            memcpy(dst8_avx,  dst8_ref,  sizeof(*dst8_ref)  * dstSize);
            memcpy(dst16_avx, dst16_ref, sizeof(*dst16_ref) * dstSize);

            const Ipp16s *fx0 = av1_filter_kernels[0][0];
            const Ipp16s *fy0 = av1_filter_kernels[0][0];

            for (int filter = 0; filter < 4; filter++) {
                for (int dx = 1; dx < 16; dx++) {
                    std::ostringstream buf;
                    buf << "Testing " << w << "x" << h << " filter=" << filter << " dx=" << dx;
                    SCOPED_TRACE(buf.str().c_str());
                    const Ipp16s *fx = av1_filter_kernels[filter][dx];
                    const Ipp16s *fy = av1_filter_kernels[filter][16 - dx];
                    { SCOPED_TRACE("vert single ref");
                        AV1PP::interp_av1_px  <w, 0, 1>(src, pitchSrc, dst8_ref, pitchDst, fx0, fy, h);
                        AV1PP::interp_av1_avx2<w, 0, 1>(src, pitchSrc, dst8_avx, pitchDst, fx0, fy, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz single ref");
                        AV1PP::interp_av1_px  <w, 1, 0>(src, pitchSrc, dst8_ref, pitchDst, fx, fy0, h);
                        AV1PP::interp_av1_avx2<w, 1, 0>(src, pitchSrc, dst8_avx, pitchDst, fx, fy0, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both single ref");
                        AV1PP::interp_av1_px  <w, 1, 1>(src, pitchSrc, dst8_ref, pitchDst, fx, fy, h);
                        AV1PP::interp_av1_avx2<w, 1, 1>(src, pitchSrc, dst8_avx, pitchDst, fx, fy, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("vert first ref");
                        AV1PP::interp_av1_px  <w, 0, 1>(src, pitchSrc, dst16_ref, pitchDst, fx0, fy, h);
                        AV1PP::interp_av1_avx2<w, 0, 1>(src, pitchSrc, dst16_avx, pitchDst, fx0, fy, h);
                        ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz first ref");
                        AV1PP::interp_av1_px  <w, 1, 0>(src, pitchSrc, dst16_ref, pitchDst, fx, fy0, h);
                        AV1PP::interp_av1_avx2<w, 1, 0>(src, pitchSrc, dst16_avx, pitchDst, fx, fy0, h);
                        ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both first ref");
                        AV1PP::interp_av1_px  <w, 1, 1>(src, pitchSrc, dst16_ref, pitchDst, fx, fy, h);
                        AV1PP::interp_av1_avx2<w, 1, 1>(src, pitchSrc, dst16_avx, pitchDst, fx, fy, h);
                        ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dstSize));
                    }
                    { SCOPED_TRACE("vert second ref");
                        AV1PP::interp_av1_px  <w, 0, 1>(src, pitchSrc, ref0, pitchDst, dst8_ref, pitchDst, fx0, fy, h);
                        AV1PP::interp_av1_avx2<w, 0, 1>(src, pitchSrc, ref0, pitchDst, dst8_avx, pitchDst, fx0, fy, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz second ref");
                        AV1PP::interp_av1_px  <w, 1, 0>(src, pitchSrc, ref0, pitchDst, dst8_ref, pitchDst, fx, fy0, h);
                        AV1PP::interp_av1_avx2<w, 1, 0>(src, pitchSrc, ref0, pitchDst, dst8_avx, pitchDst, fx, fy0, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both second ref");
                        AV1PP::interp_av1_px  <w, 1, 1>(src, pitchSrc, ref0, pitchDst, dst8_ref, pitchDst, fx, fy, h);
                        AV1PP::interp_av1_avx2<w, 1, 1>(src, pitchSrc, ref0, pitchDst, dst8_avx, pitchDst, fx, fy, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                }
            }

            std::ostringstream buf;
            buf << "Testing " << w << "x" << h;
            SCOPED_TRACE(buf.str().c_str());
            { SCOPED_TRACE("copy single ref");
                AV1PP::interp_av1_px  <w, 0, 0>(src, pitchSrc, dst8_ref, pitchDst, fx0, fy0, h);
                AV1PP::interp_av1_avx2<w, 0, 0>(src, pitchSrc, dst8_avx, pitchDst, fx0, fy0, h);
                ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
            }
            { SCOPED_TRACE("copy first ref");
                AV1PP::interp_av1_px  <w, 0, 0>(src, pitchSrc, dst16_ref, pitchDst, fx0, fy0, h);
                AV1PP::interp_av1_avx2<w, 0, 0>(src, pitchSrc, dst16_avx, pitchDst, fx0, fy0, h);
                ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dstSize));
            }
            { SCOPED_TRACE("copy second ref");
                AV1PP::interp_av1_px  <w, 0, 0>(src, pitchSrc, ref0, pitchDst, dst8_ref, pitchDst, fx0, fy0, h);
                AV1PP::interp_av1_avx2<w, 0, 0>(src, pitchSrc, ref0, pitchDst, dst8_avx, pitchDst, fx0, fy0, h);
                ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
            }
        }
    }

    template <int w> void time() {
        auto src_ptr   = utils::MakeAlignedPtr<uint8_t>(srcSize, utils::AlignAvx2);
        auto ref0_ptr  = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);
        auto dst8_ptr  = utils::MakeAlignedPtr<uint8_t>(dstSize, utils::AlignAvx2);
        auto dst16_ptr = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);

        auto src_padded = src_ptr.get();
        auto src   = src_padded + 3 * pitchSrc + RoundUpPowerOf2(3, utils::AlignAvx2);
        auto ref0  = ref0_ptr.get();
        auto dst8  = dst8_ptr.get();
        auto dst16 = dst16_ptr.get();

        const int h = w; // time square blocks
        const int16_t *fx0 = vp9_filter_kernels[0][0];
        const int16_t *fy0 = vp9_filter_kernels[0][0];
        const int16_t *fx = vp9_filter_kernels[0][4];
        const int16_t *fy = vp9_filter_kernels[0][8];

        Ipp64u tref, tavx;
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 0, 0>), (src, pitchSrc, dst8, pitchDst, fx0, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 0, 0>), (src, pitchSrc, dst8, pitchDst, fx0, fy0, h));
        printf("%2dx%-2d copy single ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 0, 0>), (src, pitchSrc, dst16, pitchDst, fx0, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 0, 0>), (src, pitchSrc, dst16, pitchDst, fx0, fy0, h));
        printf("%2dx%-2d copy first ref : ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 0, 0>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx0, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 0, 0>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx0, fy0, h));
        printf("%2dx%-2d copy second ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);

        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 0, 1>), (src, pitchSrc, dst8, pitchDst, fx0, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 0, 1>), (src, pitchSrc, dst8, pitchDst, fx0, fy, h));
        printf("%2dx%-2d vert single ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 0, 1>), (src, pitchSrc, dst16, pitchDst, fx0, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 0, 1>), (src, pitchSrc, dst16, pitchDst, fx0, fy, h));
        printf("%2dx%-2d vert first ref : ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 0, 1>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx0, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 0, 1>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx0, fy, h));
        printf("%2dx%-2d vert second ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);

        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 1, 0>), (src, pitchSrc, dst8, pitchDst, fx, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 1, 0>), (src, pitchSrc, dst8, pitchDst, fx, fy0, h));
        printf("%2dx%-2d horz single ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 1, 0>), (src, pitchSrc, dst16, pitchDst, fx, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 1, 0>), (src, pitchSrc, dst16, pitchDst, fx, fy0, h));
        printf("%2dx%-2d horz first ref : ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 1, 0>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 1, 0>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx, fy0, h));
        printf("%2dx%-2d horz second ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);

        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 1, 1>), (src, pitchSrc, dst8, pitchDst, fx, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 1, 1>), (src, pitchSrc, dst8, pitchDst, fx, fy, h));
        printf("%2dx%-2d both single ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 1, 1>), (src, pitchSrc, dst16, pitchDst, fx, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 1, 1>), (src, pitchSrc, dst16, pitchDst, fx, fy, h));
        printf("%2dx%-2d both first ref : ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_av1_px  <w, 1, 1>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_av1_avx2<w, 1, 1>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx, fy, h));
        printf("%2dx%-2d both second ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
    }
};

TEST(optimization, interp_av1)
{
    srand(0x1234);
    interp_av1::test< 4, 4,8>();
    interp_av1::test< 8, 4,8,16>();
    interp_av1::test<16, 8,16,32>();
    interp_av1::test<32, 16,32,64>();
    interp_av1::test<64, 32,64>();

    if (printTicks) {
        interp_av1::time<4>();
        interp_av1::time<8>();
        interp_av1::time<16>();
        interp_av1::time<32>();
        interp_av1::time<64>();
    }
}


namespace interp_nv12_av1 {
    using AV1Enc::av1_filter_kernels;

    const int sw = RoundUpPowerOf2(64 + 7, utils::AlignAvx2);
    const int sh = 64 + 7;
    const int dw = 64;
    const int dh = 64;
    const int pitchSrc = sw;
    const int pitchDst = dw;
    const int srcSize = pitchSrc * sh;
    const int dstSize = pitchDst * dh;

    template <int w, int... heights> void test() {
        auto src_ptr       = utils::MakeAlignedPtr<uint8_t>(srcSize, utils::AlignAvx2);
        auto ref0_ptr      = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);
        auto dst8_ref_ptr  = utils::MakeAlignedPtr<uint8_t>(dstSize, utils::AlignAvx2);
        auto dst8_avx_ptr  = utils::MakeAlignedPtr<uint8_t>(dstSize, utils::AlignAvx2);
        auto dst16_ref_ptr = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);
        auto dst16_avx_ptr = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);

        auto src_padded = src_ptr.get();
        auto src        = src_padded + 3 * pitchSrc + RoundUpPowerOf2(3, utils::AlignAvx2);
        auto ref0       = ref0_ptr.get();
        auto dst8_ref   = dst8_ref_ptr.get();
        auto dst8_avx   = dst8_avx_ptr.get();
        auto dst16_ref  = dst16_ref_ptr.get();
        auto dst16_avx  = dst16_avx_ptr.get();

        int height[sizeof...(heights)] = {heights...};

        for (const int h: height) {
            utils::InitRandomBuffer(src_padded, srcSize, 8);
            utils::InitRandomDiffBuffer(ref0, dstSize, 12);
            utils::InitRandomBuffer(dst8_ref, dstSize, 8);
            utils::InitRandomDiffBuffer(dst16_ref, dstSize, 15);
            memcpy(dst8_avx,  dst8_ref,  sizeof(*dst8_ref)  * dstSize);
            memcpy(dst16_avx, dst16_ref, sizeof(*dst16_ref) * dstSize);

            const Ipp16s *fx0 = av1_filter_kernels[0][0];
            const Ipp16s *fy0 = av1_filter_kernels[0][0];

            for (int filter = 0; filter < 4; filter++) {
                for (int dx = 1; dx < 16; dx++) {
                    std::ostringstream buf;
                    buf << "Testing " << w << "x" << h << " filter=" << filter << " dx=" << dx;
                    SCOPED_TRACE(buf.str().c_str());
                    const Ipp16s *fx = av1_filter_kernels[filter][dx];
                    const Ipp16s *fy = av1_filter_kernels[filter][16 - dx];
                    { SCOPED_TRACE("vert single ref");
                        AV1PP::interp_nv12_av1_px  <w, 0, 1>(src, pitchSrc, dst8_ref, pitchDst, fx0, fy, h);
                        AV1PP::interp_nv12_av1_avx2<w, 0, 1>(src, pitchSrc, dst8_avx, pitchDst, fx0, fy, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz single ref");
                        AV1PP::interp_nv12_av1_px  <w, 1, 0>(src, pitchSrc, dst8_ref, pitchDst, fx, fy0, h);
                        AV1PP::interp_nv12_av1_avx2<w, 1, 0>(src, pitchSrc, dst8_avx, pitchDst, fx, fy0, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both single ref");
                        AV1PP::interp_nv12_av1_px  <w, 1, 1>(src, pitchSrc, dst8_ref, pitchDst, fx, fy, h);
                        AV1PP::interp_nv12_av1_avx2<w, 1, 1>(src, pitchSrc, dst8_avx, pitchDst, fx, fy, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("vert first ref");
                        AV1PP::interp_nv12_av1_px  <w, 0, 1>(src, pitchSrc, dst16_ref, pitchDst, fx0, fy, h);
                        AV1PP::interp_nv12_av1_avx2<w, 0, 1>(src, pitchSrc, dst16_avx, pitchDst, fx0, fy, h);
                        ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz first ref");
                        AV1PP::interp_nv12_av1_px  <w, 1, 0>(src, pitchSrc, dst16_ref, pitchDst, fx, fy0, h);
                        AV1PP::interp_nv12_av1_avx2<w, 1, 0>(src, pitchSrc, dst16_avx, pitchDst, fx, fy0, h);
                        ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both first ref");
                        AV1PP::interp_nv12_av1_px  <w, 1, 1>(src, pitchSrc, dst16_ref, pitchDst, fx, fy, h);
                        AV1PP::interp_nv12_av1_avx2<w, 1, 1>(src, pitchSrc, dst16_avx, pitchDst, fx, fy, h);
                        ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dstSize));
                    }
                    { SCOPED_TRACE("vert second ref");
                        AV1PP::interp_nv12_av1_px  <w, 0, 1>(src, pitchSrc, ref0, pitchDst, dst8_ref, pitchDst, fx0, fy, h);
                        AV1PP::interp_nv12_av1_avx2<w, 0, 1>(src, pitchSrc, ref0, pitchDst, dst8_avx, pitchDst, fx0, fy, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("horz second ref");
                        AV1PP::interp_nv12_av1_px  <w, 1, 0>(src, pitchSrc, ref0, pitchDst, dst8_ref, pitchDst, fx, fy0, h);
                        AV1PP::interp_nv12_av1_avx2<w, 1, 0>(src, pitchSrc, ref0, pitchDst, dst8_avx, pitchDst, fx, fy0, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                    { SCOPED_TRACE("both second ref");
                        AV1PP::interp_nv12_av1_px  <w, 1, 1>(src, pitchSrc, ref0, pitchDst, dst8_ref, pitchDst, fx, fy, h);
                        AV1PP::interp_nv12_av1_avx2<w, 1, 1>(src, pitchSrc, ref0, pitchDst, dst8_avx, pitchDst, fx, fy, h);
                        ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
                    }
                }
            }

            std::ostringstream buf;
            buf << "Testing " << w << "x" << h;
            SCOPED_TRACE(buf.str().c_str());
            { SCOPED_TRACE("copy single ref");
                AV1PP::interp_nv12_av1_px  <w, 0, 0>(src, pitchSrc, dst8_ref, pitchDst, fx0, fy0, h);
                AV1PP::interp_nv12_av1_avx2<w, 0, 0>(src, pitchSrc, dst8_avx, pitchDst, fx0, fy0, h);
                ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
            }
            { SCOPED_TRACE("copy first ref");
                AV1PP::interp_nv12_av1_px  <w, 0, 0>(src, pitchSrc, dst16_ref, pitchDst, fx0, fy0, h);
                AV1PP::interp_nv12_av1_avx2<w, 0, 0>(src, pitchSrc, dst16_avx, pitchDst, fx0, fy0, h);
                ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dstSize));
            }
            { SCOPED_TRACE("copy second ref");
                AV1PP::interp_nv12_av1_px  <w, 0, 0>(src, pitchSrc, ref0, pitchDst, dst8_ref, pitchDst, fx0, fy0, h);
                AV1PP::interp_nv12_av1_avx2<w, 0, 0>(src, pitchSrc, ref0, pitchDst, dst8_avx, pitchDst, fx0, fy0, h);
                ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dstSize));
            }
        }
    }

    template <int w> void time() {
        auto src_ptr   = utils::MakeAlignedPtr<uint8_t>(srcSize, utils::AlignAvx2);
        auto ref0_ptr  = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);
        auto dst8_ptr  = utils::MakeAlignedPtr<uint8_t>(dstSize, utils::AlignAvx2);
        auto dst16_ptr = utils::MakeAlignedPtr<int16_t>(dstSize, utils::AlignAvx2);

        auto src_padded = src_ptr.get();
        auto src   = src_padded + 3 * pitchSrc + RoundUpPowerOf2(3, utils::AlignAvx2);
        auto ref0  = ref0_ptr.get();
        auto dst8  = dst8_ptr.get();
        auto dst16 = dst16_ptr.get();

        const int h = w; // time square blocks
        const int16_t *fx0 = vp9_filter_kernels[0][0];
        const int16_t *fy0 = vp9_filter_kernels[0][0];
        const int16_t *fx = vp9_filter_kernels[0][4];
        const int16_t *fy = vp9_filter_kernels[0][8];

        Ipp64u tref, tavx;
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 0, 0>), (src, pitchSrc, dst8, pitchDst, fx0, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 0, 0>), (src, pitchSrc, dst8, pitchDst, fx0, fy0, h));
        printf("%2dx%-2d copy single ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 0, 0>), (src, pitchSrc, dst16, pitchDst, fx0, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 0, 0>), (src, pitchSrc, dst16, pitchDst, fx0, fy0, h));
        printf("%2dx%-2d copy first ref : ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 0, 0>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx0, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 0, 0>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx0, fy0, h));
        printf("%2dx%-2d copy second ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);

        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 0, 1>), (src, pitchSrc, dst8, pitchDst, fx0, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 0, 1>), (src, pitchSrc, dst8, pitchDst, fx0, fy, h));
        printf("%2dx%-2d vert single ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 0, 1>), (src, pitchSrc, dst16, pitchDst, fx0, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 0, 1>), (src, pitchSrc, dst16, pitchDst, fx0, fy, h));
        printf("%2dx%-2d vert first ref : ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 0, 1>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx0, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 0, 1>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx0, fy, h));
        printf("%2dx%-2d vert second ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);

        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 1, 0>), (src, pitchSrc, dst8, pitchDst, fx, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 1, 0>), (src, pitchSrc, dst8, pitchDst, fx, fy0, h));
        printf("%2dx%-2d horz single ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 1, 0>), (src, pitchSrc, dst16, pitchDst, fx, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 1, 0>), (src, pitchSrc, dst16, pitchDst, fx, fy0, h));
        printf("%2dx%-2d horz first ref : ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 1, 0>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx, fy0, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 1, 0>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx, fy0, h));
        printf("%2dx%-2d horz second ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);

        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 1, 1>), (src, pitchSrc, dst8, pitchDst, fx, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 1, 1>), (src, pitchSrc, dst8, pitchDst, fx, fy, h));
        printf("%2dx%-2d both single ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 1, 1>), (src, pitchSrc, dst16, pitchDst, fx, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 1, 1>), (src, pitchSrc, dst16, pitchDst, fx, fy, h));
        printf("%2dx%-2d both first ref : ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
        GET_MIN_TICKS(tref, 100000/w,  (AV1PP::interp_nv12_av1_px  <w, 1, 1>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx, fy, h));
        GET_MIN_TICKS(tavx, 1000000/w, (AV1PP::interp_nv12_av1_avx2<w, 1, 1>), (src, pitchSrc, ref0, pitchDst, dst8, pitchDst, fx, fy, h));
        printf("%2dx%-2d both second ref: ref=%-6lld avx2=%-4lld\n", w, h, tref, tavx);
    }
};

TEST(optimization, interp_nv12_av1)
{
    srand(0x1234);

    interp_nv12_av1::test< 4, 4,8>();
    interp_nv12_av1::test< 8, 4,8,16>();
    interp_nv12_av1::test<16, 8,16,32>();
    interp_nv12_av1::test<32, 16,32>();

    if (printTicks) {
        interp_nv12_av1::time<4>();
        interp_nv12_av1::time<8>();
        interp_nv12_av1::time<16>();
        interp_nv12_av1::time<32>();
    }
}


TEST(optimization, satd)
{
    const int pitch = 64;
    const int size = 16 * pitch;

    auto src0_ptr = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignCache);
    auto src1_ptr = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignCache);
    auto src0 = src0_ptr.get();
    auto src1 = src1_ptr.get();
    Ipp32s satd_ref[2];
    Ipp32s satd_tst[2];

    srand(0x1234);

    for (Ipp32s i = 0; i < 100; i++) {
        utils::InitRandomBlock(src0, pitch, 16, 16, 8);
        utils::InitRandomBlock(src1, pitch, 16, 16, 8);

        EXPECT_EQ(AV1PP::satd_4x4_px(src0, pitch, src1, pitch), AV1PP::satd_4x4_avx2(src0, pitch, src1, pitch));
        EXPECT_EQ(AV1PP::satd_8x8_px(src0, pitch, src1, pitch), AV1PP::satd_8x8_avx2(src0, pitch, src1, pitch));

        AV1PP::satd_4x4_pair_px  (src0, pitch, src1, pitch, satd_ref);
        AV1PP::satd_4x4_pair_avx2(src0, pitch, src1, pitch, satd_tst);
        EXPECT_EQ(satd_ref[0], satd_tst[0]);
        EXPECT_EQ(satd_ref[1], satd_tst[1]);

        AV1PP::satd_8x8_pair_px  (src0, pitch, src1, pitch, satd_ref);
        AV1PP::satd_8x8_pair_avx2(src0, pitch, src1, pitch, satd_tst);
        EXPECT_EQ(satd_ref[0], satd_tst[0]);
        EXPECT_EQ(satd_ref[1], satd_tst[1]);

        AV1PP::satd_8x8x2_px  (src0, pitch, src1, pitch, satd_ref);
        AV1PP::satd_8x8x2_avx2(src0, pitch, src1, pitch, satd_tst);
        EXPECT_EQ(satd_ref[0], satd_tst[0]);
        EXPECT_EQ(satd_ref[1], satd_tst[1]);
    }

    if (printTicks) {
        Ipp64u ticksAvx2, ticksPx;
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::satd_4x4_px,   src0, pitch, src1, pitch);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::satd_4x4_avx2, src0, pitch, src1, pitch);
        printf("satd_4x4      ref=%-5lld avx2=%-5lld\n", ticksPx, ticksAvx2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::satd_8x8_px,   src0, pitch, src1, pitch);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::satd_8x8_avx2, src0, pitch, src1, pitch);
        printf("satd_8x8      ref=%-5lld avx2=%-5lld\n", ticksPx, ticksAvx2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::satd_4x4_pair_px,   src0, pitch, src1, pitch, satd_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::satd_4x4_pair_avx2, src0, pitch, src1, pitch, satd_tst);
        printf("satd_4x4_pair ref=%-5lld avx2=%-5lld\n", ticksPx, ticksAvx2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::satd_8x8_pair_px,   src0, pitch, src1, pitch, satd_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::satd_8x8_pair_avx2, src0, pitch, src1, pitch, satd_tst);
        printf("satd_8x8_pair ref=%-5lld avx2=%-5lld\n", ticksPx, ticksAvx2);
        ticksPx   = utils::GetMinTicks(1000000, AV1PP::satd_8x8x2_px,   src0, pitch, src1, pitch, satd_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::satd_8x8x2_avx2, src0, pitch, src1, pitch, satd_tst);
        printf("satd_8x8x2    ref=%-5lld avx2=%-5lld\n", ticksPx, ticksAvx2);
    }

}


namespace cdef {
    std::string make_scope_trace_string(const char *descr, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping) {
        std::ostringstream buf;
        buf << "Testing " << descr
            << " pri_strength=" << pri_strength << " sec_strength=" << sec_strength
            << " dir=" << dir << " pri_damping=" << pri_damping
            << " sec_damping=" << sec_damping;
        return buf.str();
    }
    std::string make_scope_trace_string(const char *descr, int sec_damping) {
        std::ostringstream buf;
        buf << "Testing " << descr << " sec_damping=" << sec_damping;
        return buf.str();
    }
};

TEST(optimization, cdef_find_dir)
{
    const int CDEF_VBORDER = 2;
    const int CDEF_HBORDER = 16;
    const int CDEF_BLOCKSIZE = 64;
    const int CDEF_BSTRIDE = utils::RoundUpPowerOf2(64 + 2 * CDEF_HBORDER, 8);
    const int src_block_size = CDEF_BSTRIDE * (64 + 2 * CDEF_VBORDER);

    auto src_all_ptr = utils::MakeAlignedPtr<uint16_t>(src_block_size, utils::AlignSse4);
    auto src_all = src_all_ptr.get();
    auto src = src_all + CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER;

    srand(0x1234);

    int dirs[8] = {};
    int dirs_covered = 0;
    int var_ref, dir_ref, var_avx, dir_avx;
    for (int i = 0; i < 200 || dirs_covered != 8; i++) {
        utils::InitRandomBuffer(src, src_block_size, 8);

        dir_ref = AV1PP::cdef_find_dir_px  (src, CDEF_BSTRIDE, &var_ref, 0);
        dir_avx = AV1PP::cdef_find_dir_avx2(src, CDEF_BSTRIDE, &var_avx, 0);
        ASSERT_EQ(dir_ref, dir_avx);
        ASSERT_EQ(var_ref, var_avx);

        if (!dirs[dir_ref])
            dirs_covered++;
        dirs[dir_ref] = 1;
    }

    if (printTicks) {
        Ipp64u ticksPx   = utils::GetMinTicks(1000000,  AV1PP::cdef_find_dir_px  , src, CDEF_BSTRIDE, &var_ref, 0);
        Ipp64u ticksAvx2 = utils::GetMinTicks(10000000, AV1PP::cdef_find_dir_avx2, src, CDEF_BSTRIDE, &var_avx, 0);
        printf("px=%-5lld avx2=%-5lld\n", ticksPx, ticksAvx2);
    }
}


TEST(optimization, cdef)
{
    const int CDEF_VBORDER = 2;
    const int CDEF_HBORDER = 8;
    const int CDEF_BLOCKSIZE = 64;
    const int CDEF_BSTRIDE = utils::RoundUpPowerOf2(64 + 2 * CDEF_HBORDER, 8);
    const int src_block_size = CDEF_BSTRIDE * (64 + 2 * CDEF_VBORDER);
    const int dst_block_size = 64 * 64;
    const int org_block_size = 64 * 64;
    const int dst_pitch = 64;
    const int org_pitch = 64;

    auto src_all_ptr = utils::MakeAlignedPtr<uint16_t>(src_block_size, utils::AlignSse4);
    auto org_ptr     = utils::MakeAlignedPtr<uint8_t>(org_block_size, utils::AlignSse4);
    auto dst8_ref_ptr = utils::MakeAlignedPtr<uint8_t>(dst_block_size, utils::AlignSse4);
    auto dst8_avx_ptr = utils::MakeAlignedPtr<uint8_t>(dst_block_size, utils::AlignSse4);
    auto dst16_ref_ptr = utils::MakeAlignedPtr<uint16_t>(dst_block_size, utils::AlignSse4);
    auto dst16_avx_ptr = utils::MakeAlignedPtr<uint16_t>(dst_block_size, utils::AlignSse4);
    auto src_all = src_all_ptr.get();
    auto src = src_all + CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER;
    auto org = org_ptr.get();
    auto dst8_ref = dst8_ref_ptr.get();
    auto dst8_avx = dst8_avx_ptr.get();
    auto dst16_ref = dst16_ref_ptr.get();
    auto dst16_avx = dst16_avx_ptr.get();
    int sse_ref[8];
    int sse_avx[8];

    srand(0x1234);

    utils::InitRandomBuffer(org, org_block_size, 8);

    int pri_strengths[17] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 19 };
    int sec_strengths[5] = { 0, 1, 2, 4, 7 };

    for (int pri_strength: pri_strengths) {
        for (int sec_strength: sec_strengths) {
            const int max_delta = 2 * std::max(pri_strength, sec_strength) + 1;
            for (int dir = 0; dir < 8; dir++) {
                for (int pri_damping = 3; pri_damping <= 6; pri_damping++) {
                    for (int sec_damping = 3; sec_damping <= 6; sec_damping++) {
                        utils::InitRandomBlock(src_all, CDEF_BSTRIDE, 8 + 2 * CDEF_HBORDER, 8 + 2 * CDEF_VBORDER, 128 - max_delta, 128 + max_delta);
                        memset(dst8_ref,  0xee, sizeof(*dst8_ref)  * dst_block_size);
                        memset(dst8_avx,  0xee, sizeof(*dst8_avx)  * dst_block_size);
                        memset(dst16_ref, 0xee, sizeof(*dst16_ref) * dst_block_size);
                        memset(dst16_avx, 0xee, sizeof(*dst16_avx) * dst_block_size);
                        for (int i = 0; i < 8; i++)
                            sse_ref[i] = i + 1, sse_avx[i] = i + 1 + 8;

                        { SCOPED_TRACE(cdef::make_scope_trace_string("4x4 8bit", pri_strength, sec_strength, dir, pri_damping, sec_damping).c_str());
                            AV1PP::cdef_filter_block_4x4_px  (dst8_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            AV1PP::cdef_filter_block_4x4_avx2(dst8_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dst_block_size));
                        }
                        { SCOPED_TRACE(cdef::make_scope_trace_string("8x4 8bit", pri_strength, sec_strength, dir, pri_damping, sec_damping).c_str());
                            AV1PP::cdef_filter_block_4x4_nv12_px  (dst8_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            AV1PP::cdef_filter_block_4x4_nv12_avx2(dst8_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dst_block_size));
                        }
                        { SCOPED_TRACE(cdef::make_scope_trace_string("8x8 8bit", pri_strength, sec_strength, dir, pri_damping, sec_damping).c_str());
                            AV1PP::cdef_filter_block_8x8_px  (dst8_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            AV1PP::cdef_filter_block_8x8_avx2(dst8_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            ASSERT_EQ(0, memcmp(dst8_ref, dst8_avx, sizeof(*dst8_ref) * dst_block_size));
                        }
                        { SCOPED_TRACE(cdef::make_scope_trace_string("4x4 16bit", pri_strength, sec_strength, dir, pri_damping, sec_damping).c_str());
                            AV1PP::cdef_filter_block_4x4_px  (dst16_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            AV1PP::cdef_filter_block_4x4_avx2(dst16_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dst_block_size));
                        }
                        { SCOPED_TRACE(cdef::make_scope_trace_string("8x4 16bit", pri_strength, sec_strength, dir, pri_damping, sec_damping).c_str());
                            AV1PP::cdef_filter_block_4x4_nv12_px  (dst16_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            AV1PP::cdef_filter_block_4x4_nv12_avx2(dst16_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dst_block_size));
                        }
                        { SCOPED_TRACE(cdef::make_scope_trace_string("8x8 16bit", pri_strength, sec_strength, dir, pri_damping, sec_damping).c_str());
                            AV1PP::cdef_filter_block_8x8_px  (dst16_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            AV1PP::cdef_filter_block_8x8_avx2(dst16_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
                            ASSERT_EQ(0, memcmp(dst16_ref, dst16_avx, sizeof(*dst16_ref) * dst_block_size));
                        }

                        { SCOPED_TRACE(cdef::make_scope_trace_string("4x4 sse", pri_strength, sec_strength, dir, pri_damping, sec_damping).c_str());
                            AV1PP::cdef_estimate_block_4x4_px  (org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_ref);
                            AV1PP::cdef_estimate_block_4x4_avx2(org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_avx);
                            ASSERT_EQ(sse_ref[0], sse_avx[0]);
                        }
                        { SCOPED_TRACE(cdef::make_scope_trace_string("8x4 sse", pri_strength, sec_strength, dir, pri_damping, sec_damping).c_str());
                            //AV1PP::cdef_estimate_block_4x4_nv12_px  (org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_ref);
                            //AV1PP::cdef_estimate_block_4x4_nv12_avx2(org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_avx);
                            //ASSERT_EQ(sse_ref[0], sse_avx[0]);
                        }
                        { SCOPED_TRACE(cdef::make_scope_trace_string("8x8 sse", pri_strength, sec_strength, dir, pri_damping, sec_damping).c_str());
                            //AV1PP::cdef_estimate_block_8x8_px  (org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_ref);
                            //AV1PP::cdef_estimate_block_8x8_avx2(org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_avx);
                            //ASSERT_EQ(sse_ref[0], sse_avx[0]);
                        }
                        if (sec_strength == 0 && pri_damping == 3 && sec_damping == 3) {
                            { SCOPED_TRACE(cdef::make_scope_trace_string("8x4 sse sec0", pri_strength, 0, dir, pri_damping, 3).c_str());
                                //AV1PP::cdef_estimate_block_4x4_nv12_sec0_px  (org, org_pitch, src, pri_strength, dir, pri_damping, sse_ref);
                                //AV1PP::cdef_estimate_block_4x4_nv12_sec0_avx2(org, org_pitch, src, pri_strength, dir, pri_damping, sse_avx);
                                //ASSERT_EQ(sse_ref[0], sse_avx[0]);
                            }
                            { SCOPED_TRACE(cdef::make_scope_trace_string("8x8 sse sec0", pri_strength, 0, dir, pri_damping, 3).c_str());
                                //AV1PP::cdef_estimate_block_8x8_sec0_px  (org, org_pitch, src, pri_strength, dir, pri_damping, sse_ref);
                                //AV1PP::cdef_estimate_block_8x8_sec0_avx2(org, org_pitch, src, pri_strength, dir, pri_damping, sse_avx);
                                //ASSERT_EQ(sse_ref[0], sse_avx[0]);
                            }
                        }
                        if (pri_strength == 0 && sec_strength == 0 && dir == 0 && pri_damping == 3) {
                            { SCOPED_TRACE(cdef::make_scope_trace_string("4x4 sse pri0", sec_damping).c_str());
                                AV1PP::cdef_estimate_block_4x4_pri0_px  (org, org_pitch, src, sec_damping, sse_ref);
                                AV1PP::cdef_estimate_block_4x4_pri0_avx2(org, org_pitch, src, sec_damping, sse_avx);
                                for (int i = 0; i < 3; i++)
                                    ASSERT_EQ(sse_ref[i], sse_avx[i]);
                            }
                            { SCOPED_TRACE(cdef::make_scope_trace_string("8x4 sse pri0", sec_damping).c_str());
                                AV1PP::cdef_estimate_block_4x4_nv12_pri0_px  (org, org_pitch, src, sec_damping, sse_ref);
                                AV1PP::cdef_estimate_block_4x4_nv12_pri0_avx2(org, org_pitch, src, sec_damping, sse_avx);
                                for (int i = 0; i < 3; i++)
                                    ASSERT_EQ(sse_ref[i], sse_avx[i]);
                            }
                            { SCOPED_TRACE(cdef::make_scope_trace_string("8x8 sse pri0", sec_damping).c_str());
                                AV1PP::cdef_estimate_block_8x8_pri0_px  (org, org_pitch, src, sec_damping, sse_ref);
                                AV1PP::cdef_estimate_block_8x8_pri0_avx2(org, org_pitch, src, sec_damping, sse_avx);
                                for (int i = 0; i < 3; i++)
                                    ASSERT_EQ(sse_ref[i], sse_avx[i]);
                            }
                        }
                        if (sec_strength == 0) {
                            { SCOPED_TRACE(cdef::make_scope_trace_string("4x4 sse all_sec", pri_strength, -1, dir, pri_damping, sec_damping).c_str());
                                AV1PP::cdef_estimate_block_4x4_all_sec_px  (org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_ref);
                                AV1PP::cdef_estimate_block_4x4_all_sec_avx2(org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_avx);
                                for (int i = 0; i < 4; i++)
                                    ASSERT_EQ(sse_ref[i], sse_avx[i]);
                            }
                            { SCOPED_TRACE(cdef::make_scope_trace_string("8x4 sse all_sec", pri_strength, -1, dir, pri_damping, sec_damping).c_str());
                                //AV1PP::cdef_estimate_block_4x4_nv12_all_sec_px  (org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_ref);
                                //AV1PP::cdef_estimate_block_4x4_nv12_all_sec_avx2(org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_avx);
                                //for (int i = 0; i < 4; i++)
                                //    ASSERT_EQ(sse_ref[i], sse_avx[i]);
                            }
                            { SCOPED_TRACE(cdef::make_scope_trace_string("8x8 sse all_sec", pri_strength, -1, dir, pri_damping, sec_damping).c_str());
                                //AV1PP::cdef_estimate_block_8x8_all_sec_px  (org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_ref);
                                //AV1PP::cdef_estimate_block_8x8_all_sec_avx2(org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_avx);
                                //for (int i = 0; i < 4; i++)
                                //    ASSERT_EQ(sse_ref[i], sse_avx[i]);
                            }
                            { SCOPED_TRACE(cdef::make_scope_trace_string("4x4 sse 2pri", pri_strength, -1, dir, pri_damping, sec_damping).c_str());
                                const int pri_strength0 = pri_strength;
                                const int pri_strength1 = (pri_strength + 15) & 15;
                                AV1PP::cdef_estimate_block_4x4_2pri_px  (org, org_pitch, src, pri_strength0, pri_strength1, dir, pri_damping, sec_damping, sse_ref);
                                AV1PP::cdef_estimate_block_4x4_2pri_avx2(org, org_pitch, src, pri_strength0, pri_strength1, dir, pri_damping, sec_damping, sse_avx);
                                for (int i = 0; i < 8; i++)
                                    ASSERT_EQ(sse_ref[i], sse_avx[i]);
                            }
                            { SCOPED_TRACE(cdef::make_scope_trace_string("8x4 sse 2pri", pri_strength, -1, dir, pri_damping, sec_damping).c_str());
                                const int pri_strength0 = pri_strength;
                                const int pri_strength1 = (pri_strength + 15) & 15;
                                AV1PP::cdef_estimate_block_4x4_nv12_2pri_px  (org, org_pitch, src, pri_strength0, pri_strength1, dir, pri_damping, sec_damping, sse_ref);
                                AV1PP::cdef_estimate_block_4x4_nv12_2pri_avx2(org, org_pitch, src, pri_strength0, pri_strength1, dir, pri_damping, sec_damping, sse_avx);
                                for (int i = 0; i < 8; i++)
                                    ASSERT_EQ(sse_ref[i], sse_avx[i]);
                            }
                            { SCOPED_TRACE(cdef::make_scope_trace_string("8x8 sse 2pri", pri_strength, -1, dir, pri_damping, sec_damping).c_str());
                                const int pri_strength0 = pri_strength;
                                const int pri_strength1 = (pri_strength + 15) & 15;
                                AV1PP::cdef_estimate_block_8x8_2pri_px  (org, org_pitch, src, pri_strength0, pri_strength1, dir, pri_damping, sec_damping, sse_ref);
                                AV1PP::cdef_estimate_block_8x8_2pri_avx2(org, org_pitch, src, pri_strength0, pri_strength1, dir, pri_damping, sec_damping, sse_avx);
                                for (int i = 0; i < 8; i++)
                                    ASSERT_EQ(sse_ref[i], sse_avx[i]);
                            }
                        }
                    }
                }
            }
        }
    }

    if (printTicks) {
        const int pri_strength = 10;
        const int sec_strength = 2;
        const int dir = 2;
        const int pri_damping = 3;
        const int sec_damping = 4;
        const int max_delta = 2 * std::max(pri_strength, sec_strength) + 1;
        utils::InitRandomBuffer(src_all, src_block_size, (1 << 7) - max_delta, (1 << 7) + max_delta);

        Ipp64u ticksPx, ticksAvx2;
        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_filter_block_4x4_px  <Ipp8u>, dst8_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_filter_block_4x4_avx2<Ipp8u>, dst8_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        printf("4x4 8bit  pri_str=%-2d sec_str=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_strength, sec_strength, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_filter_block_4x4_nv12_px  <Ipp8u>, dst8_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_filter_block_4x4_nv12_avx2<Ipp8u>, dst8_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        printf("8x4 8bit  pri_str=%-2d sec_str=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_strength, sec_strength, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_filter_block_8x8_px  <Ipp8u>, dst8_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_filter_block_8x8_avx2<Ipp8u>, dst8_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        printf("8x8 8bit  pri_str=%-2d sec_str=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_strength, sec_strength, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_filter_block_4x4_px  <Ipp16u>, dst16_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_filter_block_4x4_avx2<Ipp16u>, dst16_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        printf("4x4 16bit pri_str=%-2d sec_str=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_strength, sec_strength, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_filter_block_4x4_nv12_px  <Ipp16u>, dst16_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_filter_block_4x4_nv12_avx2<Ipp16u>, dst16_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        printf("8x4 16bit pri_str=%-2d sec_str=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_strength, sec_strength, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_filter_block_8x8_px  <Ipp16u>, dst16_ref, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_filter_block_8x8_avx2<Ipp16u>, dst16_avx, dst_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
        printf("8x8 16bit pri_str=%-2d sec_str=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_strength, sec_strength, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_4x4_px,   org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_4x4_avx2, org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_avx);
        printf("4x4 sse   pri_str=%-2d sec_str=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_strength, sec_strength, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        //ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_4x4_nv12_px,   org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_ref);
        //ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_4x4_nv12_avx2, org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_avx);
        //printf("8x4 sse   pri_str=%-2d sec_str=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_strength, sec_strength, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        //ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_8x8_px,   org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_ref);
        //ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_8x8_avx2, org, org_pitch, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse_avx);
        //printf("8x8 sse   pri_str=%-2d sec_str=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_strength, sec_strength, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        //ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_4x4_nv12_sec0_px,   org, org_pitch, src, pri_strength, dir, pri_damping, sse_ref);
        //ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_4x4_nv12_sec0_avx2, org, org_pitch, src, pri_strength, dir, pri_damping, sse_avx);
        //printf("8x8 sse sec0 pri_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_damping, ticksPx, ticksAvx2);

        //ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_8x8_sec0_px,   org, org_pitch, src, pri_strength, dir, pri_damping, sse_ref);
        //ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_8x8_sec0_avx2, org, org_pitch, src, pri_strength, dir, pri_damping, sse_avx);
        //printf("8x8 sse sec0 pri_dmp=%d : px=%-5lld avx2=%-5lld\n", pri_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_4x4_pri0_px,   org, org_pitch, src, sec_damping, sse_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_4x4_pri0_avx2, org, org_pitch, src, sec_damping, sse_avx);
        printf("4x4 sse pri0 sec_dmp=%d : px=%-5lld avx2=%-5lld\n", sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_4x4_nv12_pri0_px,   org, org_pitch, src, sec_damping, sse_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_4x4_nv12_pri0_avx2, org, org_pitch, src, sec_damping, sse_avx);
        printf("8x4 sse pri0 sec_dmp=%d : px=%-5lld avx2=%-5lld\n", sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_8x8_pri0_px,   org, org_pitch, src, sec_damping, sse_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_8x8_pri0_avx2, org, org_pitch, src, sec_damping, sse_avx);
        printf("8x8 sse pri0 sec_dmp=%d : px=%-5lld avx2=%-5lld\n", sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_4x4_all_sec_px,   org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_4x4_all_sec_avx2, org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_avx);
        printf("4x4 sse all_sec sec_dmp=%d : px=%-5lld avx2=%-5lld\n", sec_damping, ticksPx, ticksAvx2);

        //ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_4x4_nv12_all_sec_px,   org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_ref);
        //ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_4x4_nv12_all_sec_avx2, org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_avx);
        //printf("8x4 sse all_sec sec_dmp=%d : px=%-5lld avx2=%-5lld\n", sec_damping, ticksPx, ticksAvx2);

        //ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_8x8_all_sec_px,   org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_ref);
        //ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_8x8_all_sec_avx2, org, org_pitch, src, pri_strength, dir, pri_damping, sec_damping, sse_avx);
        //printf("8x8 sse all_sec sec_dmp=%d : px=%-5lld avx2=%-5lld\n", sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_4x4_2pri_px,   org, org_pitch, src, 1, 5, dir, pri_damping, sec_damping, sse_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_4x4_2pri_avx2, org, org_pitch, src, 1, 5, dir, pri_damping, sec_damping, sse_avx);
        printf("4x4 sse 2pri pri_str0=%d pri_str1=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", 1, 5, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_4x4_nv12_2pri_px,   org, org_pitch, src, 1, 5, dir, pri_damping, sec_damping, sse_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_4x4_nv12_2pri_avx2, org, org_pitch, src, 1, 5, dir, pri_damping, sec_damping, sse_avx);
        printf("8x4 sse 2pri pri_str0=%d pri_str1=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", 1, 5, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);

        ticksPx   = utils::GetMinTicks(100000,  AV1PP::cdef_estimate_block_8x8_2pri_px,   org, org_pitch, src, 1, 5, dir, pri_damping, sec_damping, sse_ref);
        ticksAvx2 = utils::GetMinTicks(1000000, AV1PP::cdef_estimate_block_8x8_2pri_avx2, org, org_pitch, src, 1, 5, dir, pri_damping, sec_damping, sse_avx);
        printf("8x8 sse 2pri pri_str0=%d pri_str1=%d dir=%d pri_dmp=%d sec_dmp=%d : px=%-5lld avx2=%-5lld\n", 1, 5, dir, pri_damping, sec_damping, ticksPx, ticksAvx2);
    }
}
