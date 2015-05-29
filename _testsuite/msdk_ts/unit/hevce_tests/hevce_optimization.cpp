#include "gtest/gtest.h"

#include "random"
#include "memory"

#define MFX_TARGET_OPTIMIZATION_AUTO
#define MFX_ENABLE_H265_VIDEO_ENCODE
#include "mfx_h265_dispatcher.h"
//#include "mfx_h265_optimization.h"
#undef min
#undef max

//#define PRINT_TICKS

namespace utils {

    enum { AlignAvx2 = 32, AlignSse4 = 16, AlignMax = AlignAvx2 };

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


TEST(optimization, SSE_avx2) {
    Ipp32s pitch1 = 64;
    Ipp32s pitch2 = 128;
    auto src1 = utils::MakeAlignedPtr<unsigned char>(pitch1 * 64, utils::AlignAvx2);
    auto src2 = utils::MakeAlignedPtr<unsigned char>(pitch2 * 64, utils::AlignAvx2);

    auto src1_10b = utils::MakeAlignedPtr<unsigned short>(pitch1 * 64, utils::AlignAvx2);
    auto src2_10b = utils::MakeAlignedPtr<unsigned short>(pitch2 * 64, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);
    utils::InitRandomBlock(rand, src1.get(), pitch1, 64, 64, 0, 255);
    utils::InitRandomBlock(rand, src2.get(), pitch2, 64, 64, 0, 255);
    utils::InitRandomBlock(rand, src1_10b.get(), pitch1, 64, 64, 0, 1023);
    utils::InitRandomBlock(rand, src2_10b.get(), pitch2, 64, 64, 0, 1023);

    //utils::InitConstBlock(src1_10b.get(), pitch1, 64, 64, 0);
    //utils::InitConstBlock(src2_10b.get(), pitch2, 64, 64, 1023);
    

    Ipp32s dims[][2] = {
        {4,4}, {4,8}, {4,16},
        {8,4}, {8,8}, {8,16}, {8,32},
        {12,16},
        {16,4}, {16,8}, {16,12}, {16,16}, {16,32}, {16,64},
        {24,32},
        {32,8}, {32,16}, {32,24}, {32,32}, {32,64},
        {48,64},
        {64,16}, {64,32}, {64,48}, {64,64}
    };

//#define PRINT_TICKS
#ifdef PRINT_TICKS
    for (auto wh: dims) {
        Ipp64u ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_avx2<Ipp8u>, src1.get(), pitch1, src2.get(), pitch2, wh[0], wh[1]);
        Ipp64u ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_px<Ipp8u>,   src1.get(), pitch1, src2.get(), pitch2, wh[0], wh[1]);
        printf("%2dx%2d  8bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);

        ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_avx2<Ipp16u>, src1_10b.get(), pitch1, src2_10b.get(), pitch2, wh[0], wh[1]);
        ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_px<Ipp16u>,   src1_10b.get(), pitch1, src2_10b.get(), pitch2, wh[0], wh[1]);
        printf("%2dx%2d 10bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);
    }
#undef PRINT_TICKS
#endif // PRINT_TICKS

    //EXPECT_LE(utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_avx2<Ipp8u>, src1.get(), pitch1, src2.get(), pitch2, 16, 16),
    //          utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_px<Ipp8u>,   src1.get(), pitch1, src2.get(), pitch2, 16, 16));

    for (auto wh: dims) {
        std::ostringstream buf;
        buf << "Testing " << wh[0] << "x" << wh[1] << " 8bit";
        SCOPED_TRACE(buf.str().c_str());
        EXPECT_EQ(MFX_HEVC_PP::h265_SSE_avx2(src1.get(), pitch1, src2.get(), pitch2, wh[0], wh[1]),
                  MFX_HEVC_PP::h265_SSE_px  (src1.get(), pitch1, src2.get(), pitch2, wh[0], wh[1]));
    }

    for (auto wh: dims) {
        std::ostringstream buf;
        buf << "Testing " << wh[0] << "x" << wh[1] << " 10bit";
        SCOPED_TRACE(buf.str().c_str());
        EXPECT_EQ(MFX_HEVC_PP::h265_SSE_avx2(src1_10b.get(), pitch1, src2_10b.get(), pitch2, wh[0], wh[1]),
                  MFX_HEVC_PP::h265_SSE_px  (src1_10b.get(), pitch1, src2_10b.get(), pitch2, wh[0], wh[1]));
    }
}


TEST(optimization, DiffNv12_avx2) {
    Ipp32s pitchSrc  = 128;
    Ipp32s pitchPred = 256;

    auto src   = utils::MakeAlignedPtr<unsigned char>(pitchSrc  * 64, utils::AlignAvx2);
    auto pred  = utils::MakeAlignedPtr<unsigned char>(pitchPred * 64, utils::AlignAvx2);

    auto diff1Tst = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto diff2Tst = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto diff1Ref = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);
    auto diff2Ref = utils::MakeAlignedPtr<short>(64 * 64, utils::AlignAvx2);

    auto src_10b   = utils::MakeAlignedPtr<unsigned short>(pitchSrc  * 64, utils::AlignAvx2);
    auto pred_10b  = utils::MakeAlignedPtr<unsigned short>(pitchPred * 64, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);
    utils::InitRandomBlock(rand, src.get(), pitchSrc, 128, 64, 0, 255);
    utils::InitRandomBlock(rand, pred.get(), pitchPred, 128, 64, 0, 255);
    utils::InitRandomBlock(rand, src_10b.get(), pitchSrc, 128, 64, 0, 1023);
    utils::InitRandomBlock(rand, pred_10b.get(), pitchPred, 128, 64, 0, 1023);

    Ipp32s dims[][2] = {
        {2, 2}, {2, 4}, {2, 8}, {2, 16},
        {4, 2}, {4, 4}, {4, 8}, {4, 16}, {4, 32},
        {6, 8}, {6, 16},
        {8, 2}, {8, 4}, {8, 6}, {8, 8}, {8, 12}, {8, 16}, {8, 32}, {8, 64},
        {12, 16}, {12, 32},
        {16, 4}, {16, 8}, {16, 12}, {16, 16}, {16, 24}, {16, 32}, {16, 64},
        {24, 32}, {24, 64},
        {32, 8}, {32, 16}, {32, 24}, {32, 32}, {32, 48}, {32, 64},
        {48, 64},
        {64, 16}, {64, 32}, {64, 48}, {64, 64}
    };

//#define PRINT_TICKS
#ifdef PRINT_TICKS
    for (auto wh: dims) {
        Ipp64u ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_avx2<Ipp8u>, src.get(), pitchSrc, pred.get(), pitchPred, diff1Tst.get(), wh[0], diff2Tst.get(), wh[0], wh[0], wh[1]);
        Ipp64u ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_px<Ipp8u>,   src.get(), pitchSrc, pred.get(), pitchPred, diff1Tst.get(), wh[0], diff2Ref.get(), wh[0], wh[0], wh[1]);
        printf("%2dx%2d  8bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);

        ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_avx2<Ipp16u>, src_10b.get(), pitchSrc, pred_10b.get(), pitchPred, diff1Tst.get(), wh[0], diff2Tst.get(), wh[0], wh[0], wh[1]);
        ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_px<Ipp16u>,   src_10b.get(), pitchSrc, pred_10b.get(), pitchPred, diff1Ref.get(), wh[0], diff2Ref.get(), wh[0], wh[0], wh[1]);
        printf("%2dx%2d 10bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);
    }
#undef PRINT_TICKS
#endif //PRINT_TICKS

    //EXPECT_LE(utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_avx2<Ipp8u>, src.get(), pitchSrc, pred.get(), pitchPred, diff1Tst.get(), 16, diff2Tst.get(), 16, 32, 16),
    //          utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_px<Ipp8u>,   src.get(), pitchSrc, pred.get(), pitchPred, diff1Ref.get(), 16, diff2Ref.get(), 16, 32, 16));

    for (auto wh: dims) {
        std::ostringstream buf;
        buf << "Testing " << wh[0] << "x" << wh[1] << " 8bit";
        SCOPED_TRACE(buf.str().c_str());
        MFX_HEVC_PP::h265_DiffNv12_avx2(src.get(), pitchSrc, pred.get(), pitchPred, diff1Tst.get(), wh[0], diff2Tst.get(), wh[0], wh[0], wh[1]);
        MFX_HEVC_PP::h265_DiffNv12_px  (src.get(), pitchSrc, pred.get(), pitchPred, diff1Ref.get(), wh[0], diff2Ref.get(), wh[0], wh[0], wh[1]);
        EXPECT_EQ(0, memcmp(diff1Tst.get(), diff1Ref.get(), sizeof(diff1Tst.get()[0]) * wh[0] * wh[1]));
        EXPECT_EQ(0, memcmp(diff2Tst.get(), diff2Ref.get(), sizeof(diff2Tst.get()[0]) * wh[0] * wh[1]));
    }

    for (auto wh: dims) {
        std::ostringstream buf;
        buf << "Testing " << wh[0] << "x" << wh[1] << " 10bit";
        SCOPED_TRACE(buf.str().c_str());
        MFX_HEVC_PP::h265_DiffNv12_avx2(src_10b.get(), pitchSrc, pred_10b.get(), pitchPred, diff1Tst.get(), wh[0], diff2Tst.get(), wh[0], wh[0], wh[1]);
        MFX_HEVC_PP::h265_DiffNv12_px  (src_10b.get(), pitchSrc, pred_10b.get(), pitchPred, diff1Ref.get(), wh[0], diff2Ref.get(), wh[0], wh[0], wh[1]);
        EXPECT_EQ(0, memcmp(diff1Tst.get(), diff1Ref.get(), sizeof(diff1Tst.get()[0]) * wh[0] * wh[1]));
        EXPECT_EQ(0, memcmp(diff2Tst.get(), diff2Ref.get(), sizeof(diff2Tst.get()[0]) * wh[0] * wh[1]));
    }
}


TEST(optimization, SplitChromaCtb_avx2) {
    Ipp32s pitchSrc  = 256;

    auto nv12 = utils::MakeAlignedPtr<unsigned char>(pitchSrc  * 64, utils::AlignAvx2);
    auto uTst = utils::MakeAlignedPtr<unsigned char>(64 * 64, utils::AlignAvx2);
    auto vTst = utils::MakeAlignedPtr<unsigned char>(64 * 64, utils::AlignAvx2);
    auto uRef = utils::MakeAlignedPtr<unsigned char>(64 * 64, utils::AlignAvx2);
    auto vRef = utils::MakeAlignedPtr<unsigned char>(64 * 64, utils::AlignAvx2);

    auto nv12_10b = utils::MakeAlignedPtr<unsigned short>(pitchSrc  * 64, utils::AlignAvx2);
    auto uTst_10b = utils::MakeAlignedPtr<unsigned short>(64 * 64, utils::AlignAvx2);
    auto vTst_10b = utils::MakeAlignedPtr<unsigned short>(64 * 64, utils::AlignAvx2);
    auto uRef_10b = utils::MakeAlignedPtr<unsigned short>(64 * 64, utils::AlignAvx2);
    auto vRef_10b = utils::MakeAlignedPtr<unsigned short>(64 * 64, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);
    utils::InitRandomBlock(rand, nv12.get(), pitchSrc, 128, 64, 0, 255);
    utils::InitRandomBlock(rand, nv12_10b.get(), pitchSrc, 128, 64, 0, 1023);

    Ipp32s dims[][2] = {
        {16, 16}, {16, 32},
        {32, 32}, {32, 64},
        {64, 64}
    };

//#define PRINT_TICKS
//#undef PRINT_TICKS
#ifdef PRINT_TICKS
    for (auto wh: dims) {
        Ipp64u ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SplitChromaCtb_avx2<Ipp8u>, nv12.get(), pitchSrc, uTst.get(), wh[0], vTst.get(), wh[0], wh[0], wh[1]);
        Ipp64u ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SplitChromaCtb_px<Ipp8u>,   nv12.get(), pitchSrc, uRef.get(), wh[0], vRef.get(), wh[0], wh[0], wh[1]);
        printf("%2dx%2d  8bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);

        ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SplitChromaCtb_avx2<Ipp16u>, nv12_10b.get(), pitchSrc, uTst_10b.get(), wh[0], vTst_10b.get(), wh[0], wh[0], wh[1]);
        ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SplitChromaCtb_px<Ipp16u>,   nv12_10b.get(), pitchSrc, uRef_10b.get(), wh[0], vRef_10b.get(), wh[0], wh[0], wh[1]);
        printf("%2dx%2d 10bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);
    }
#endif //PRINT_TICKS

    for (auto wh: dims) {
        std::ostringstream buf;
        buf << "Testing " << wh[0] << "x" << wh[1] << " 8bit";
        SCOPED_TRACE(buf.str().c_str());
        MFX_HEVC_PP::h265_SplitChromaCtb_avx2(nv12.get(), pitchSrc, uTst.get(), wh[0], vTst.get(), wh[0], wh[0], wh[1]);
        MFX_HEVC_PP::h265_SplitChromaCtb_px  (nv12.get(), pitchSrc, uRef.get(), wh[0], vRef.get(), wh[0], wh[0], wh[1]);
        EXPECT_EQ(0, memcmp(uTst.get(), uRef.get(), sizeof(*uTst.get()) * wh[0] * wh[1]));
        EXPECT_EQ(0, memcmp(vTst.get(), vRef.get(), sizeof(*vTst.get()) * wh[0] * wh[1]));
    }

    for (auto wh: dims) {
        std::ostringstream buf;
        buf << "Testing " << wh[0] << "x" << wh[1] << " 10bit";
        SCOPED_TRACE(buf.str().c_str());
        MFX_HEVC_PP::h265_SplitChromaCtb_avx2(nv12_10b.get(), pitchSrc, uTst_10b.get(), wh[0], vTst_10b.get(), wh[0], wh[0], wh[1]);
        MFX_HEVC_PP::h265_SplitChromaCtb_px  (nv12_10b.get(), pitchSrc, uRef_10b.get(), wh[0], vRef_10b.get(), wh[0], wh[0], wh[1]);
        EXPECT_EQ(0, memcmp(uTst_10b.get(), uRef_10b.get(), sizeof(*uTst_10b.get()) * wh[0] * wh[1]));
        EXPECT_EQ(0, memcmp(vTst_10b.get(), vRef_10b.get(), sizeof(*vTst_10b.get()) * wh[0] * wh[1]));
    }
}
