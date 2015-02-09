// hevce_tests.cpp : Defines the entry point for the console application.
//

#include "gtest/gtest.h"

#include "random"
#include "memory"

#define MFX_TARGET_OPTIMIZATION_AUTO
#define MFX_ENABLE_H265_VIDEO_ENCODE
#include "mfx_h265_dispatcher.h"
//#include "mfx_h265_optimization.h"
#undef min
#undef max


namespace utils {

    enum { AlignAvx2 = 32, AlignSse4 = 16, AlignMax = AlignAvx2 };

    void *AlignedMalloc(size_t size, size_t align)
    {
        align = std::max(align, sizeof(void *));
        size_t allocsize = size + align + sizeof(void *);
        void *ptr = malloc(allocsize);
        if (ptr == nullptr)
            throw std::bad_alloc();
        size_t space = allocsize - sizeof(void *);
        void *alignedPtr = (char *)ptr + sizeof(void *);
        alignedPtr = std::align(align, size, alignedPtr, space);
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
        T *ptr = static_cast<T *>(utils::AlignedMalloc(size, align));
        return std::unique_ptr<T, decltype(&utils::AlignedFree)>(ptr, &utils::AlignedFree);
    }

    template <class TRand, class T, class U>
    void InitRandomBlock(TRand &randEngine, T* block, int pitch, int width, int height, U minVal, U maxVal)
    {
        std::uniform_int_distribution<T> dist(static_cast<T>(minVal), static_cast<T>(maxVal));

        for (int r = 0; r < height; r++, block += pitch)
            for (int c = 0; c < width; c++)
                block[c] = dist(randEngine);
    }

    template <typename Func, typename... Args> mfxI64 GetMinTicks(int number, Func func, Args&&... args)
    {
        mfxI64 fastest = std::numeric_limits<mfxI64>::max();
        for (int i = 0; i < number; i++) {
            mfxI64 start = _rdtsc();
            func(args...);
            mfxI64 stop = _rdtsc();
            mfxI64 ticks = stop - start;
            fastest = std::min(fastest, ticks);
        }
        return fastest;
    }
};


TEST(optimization, SAD_avx2) {

    const int pitch = 1920;
    const int size = 128 * pitch;

    auto src = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignAvx2);
    auto refGeneral = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignAvx2);
    auto refSpecial = utils::MakeAlignedPtr<unsigned char>(64 * 64, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);
    utils::InitRandomBlock(rand, src.get(), pitch, 65, 64, 0, 255);
    utils::InitRandomBlock(rand, refGeneral.get(), pitch, 65, 64, 0, 255);
    utils::InitRandomBlock(rand, refSpecial.get(), 64, 64, 64, 0, 255);

    EXPECT_LE(utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_avx2, src.get(), refSpecial.get(), pitch),
              utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_px,   src.get(), refSpecial.get(), pitch));

    EXPECT_EQ(MFX_HEVC_PP::SAD_4x4_avx2  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_4x4_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_4x8_avx2  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_4x8_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_4x16_avx2 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_4x16_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x4_avx2  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x4_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x8_avx2  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x8_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x16_avx2 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x16_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x32_avx2 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x32_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_12x16_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_12x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x4_avx2 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x4_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x8_avx2 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x8_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x12_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x12_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x16_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x32_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x64_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x64_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_24x32_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_24x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x8_avx2 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x8_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x16_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x24_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x24_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x32_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x64_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x64_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_48x64_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_48x64_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x16_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x32_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x48_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x48_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x64_avx2(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x64_px(src.get(), refSpecial.get(), pitch));

    for (int alignment = 0; alignment <= 1; alignment++) {
        EXPECT_EQ(MFX_HEVC_PP::SAD_4x4_general_avx2  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_4x4_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_4x8_general_avx2  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_4x8_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_4x16_general_avx2 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_4x16_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x4_general_avx2  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x4_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x8_general_avx2  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x8_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x16_general_avx2 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x16_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x32_general_avx2 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x32_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_12x16_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_12x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x4_general_avx2 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x4_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x8_general_avx2 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x8_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x12_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x12_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x16_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x32_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x64_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_24x32_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_24x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x8_general_avx2 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x8_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x16_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x24_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x24_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x32_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x64_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_48x64_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_48x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x16_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x32_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x48_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x48_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x64_general_avx2(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
    }
}


TEST(optimization, SAD_sse4) {

    const int pitch = 1920;
    const int size = 64 * pitch;

    auto src = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignSse4);
    auto refGeneral = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignSse4);
    auto refSpecial = utils::MakeAlignedPtr<unsigned char>(64 * 64, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);
    utils::InitRandomBlock(rand, src.get(), pitch, 65, 64, 0, 255);
    utils::InitRandomBlock(rand, refGeneral.get(), pitch, 65, 64, 0, 255);
    utils::InitRandomBlock(rand, refSpecial.get(), 64, 64, 64, 0, 255);

    EXPECT_LE(utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_sse, src.get(), refSpecial.get(), pitch),
              utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_px,  src.get(), refSpecial.get(), pitch));

    EXPECT_EQ(MFX_HEVC_PP::SAD_4x4_sse  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_4x4_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_4x8_sse  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_4x8_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_4x16_sse (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_4x16_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x4_sse  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x4_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x8_sse  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x8_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x16_sse (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x16_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x32_sse (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x32_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_12x16_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_12x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x4_sse (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x4_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x8_sse (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x8_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x12_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x12_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x16_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x32_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x64_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x64_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_24x32_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_24x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x8_sse (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x8_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x16_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x24_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x24_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x32_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x64_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x64_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_48x64_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_48x64_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x16_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x32_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x48_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x48_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x64_sse(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x64_px(src.get(), refSpecial.get(), pitch));

    for (int alignment = 0; alignment <= 1; alignment++) {
        EXPECT_EQ(MFX_HEVC_PP::SAD_4x4_general_sse  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_4x4_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_4x8_general_sse  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_4x8_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_4x16_general_sse (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_4x16_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x4_general_sse  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x4_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x8_general_sse  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x8_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x16_general_sse (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x16_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x32_general_sse (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x32_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_12x16_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_12x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x4_general_sse (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x4_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x8_general_sse (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x8_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x12_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x12_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x16_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x32_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x64_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_24x32_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_24x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x8_general_sse (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x8_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x16_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x24_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x24_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x32_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x64_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_48x64_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_48x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x16_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x32_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x48_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x48_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x64_general_sse(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
    }
}


TEST(optimization, SAD_ssse3) {

    const int pitch = 1920;
    const int size = 64 * pitch;

    auto src = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignSse4);
    auto refGeneral = utils::MakeAlignedPtr<unsigned char>(size, utils::AlignSse4);
    auto refSpecial = utils::MakeAlignedPtr<unsigned char>(64 * 64, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);
    utils::InitRandomBlock(rand, src.get(), pitch, 65, 64, 0, 255);
    utils::InitRandomBlock(rand, refGeneral.get(), pitch, 65, 64, 0, 255);
    utils::InitRandomBlock(rand, refSpecial.get(), 64, 64, 64, 0, 255);

    EXPECT_LE(utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_ssse3, src.get(), refSpecial.get(), pitch),
              utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_px,    src.get(), refSpecial.get(), pitch));

    EXPECT_EQ(MFX_HEVC_PP::SAD_4x4_ssse3  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_4x4_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_4x8_ssse3  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_4x8_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_4x16_ssse3 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_4x16_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x4_ssse3  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x4_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x8_ssse3  (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x8_px  (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x16_ssse3 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x16_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_8x32_ssse3 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_8x32_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_12x16_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_12x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x4_ssse3 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x4_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x8_ssse3 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x8_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x12_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x12_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x16_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x32_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_16x64_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_16x64_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_24x32_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_24x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x8_ssse3 (src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x8_px (src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x16_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x24_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x24_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x32_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_32x64_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_32x64_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_48x64_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_48x64_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x16_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x16_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x32_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x32_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x48_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x48_px(src.get(), refSpecial.get(), pitch));
    EXPECT_EQ(MFX_HEVC_PP::SAD_64x64_ssse3(src.get(), refSpecial.get(), pitch), MFX_HEVC_PP::SAD_64x64_px(src.get(), refSpecial.get(), pitch));

    for (int alignment = 0; alignment <= 1; alignment++) {
        EXPECT_EQ(MFX_HEVC_PP::SAD_4x4_general_ssse3  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_4x4_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_4x8_general_ssse3  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_4x8_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_4x16_general_ssse3 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_4x16_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x4_general_ssse3  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x4_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x8_general_ssse3  (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x8_general_px  (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x16_general_ssse3 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x16_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_8x32_general_ssse3 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_8x32_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_12x16_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_12x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x4_general_ssse3 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x4_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x8_general_ssse3 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x8_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x12_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x12_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x16_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x32_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_16x64_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_16x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_24x32_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_24x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x8_general_ssse3 (refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x8_general_px (refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x16_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x24_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x24_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x32_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_32x64_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_32x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_48x64_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_48x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x16_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x16_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x32_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x32_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x48_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x48_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
        EXPECT_EQ(MFX_HEVC_PP::SAD_64x64_general_ssse3(refGeneral.get() + alignment, src.get(), pitch, pitch), MFX_HEVC_PP::SAD_64x64_general_px(refGeneral.get() + alignment, src.get(), pitch, pitch));
    }
}
