#include "gtest/gtest.h"

#include "random"
#include "memory"

#define MFX_TARGET_OPTIMIZATION_AUTO
#define MFX_ENABLE_H265_VIDEO_ENCODE
#include "mfx_h265_dispatcher.h"
//#include "mfx_h265_optimization.h"
#undef min
#undef max

#define PRINT_TICKS
#undef PRINT_TICKS

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

#ifdef PRINT_TICKS
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
#endif // PRINT_TICKS
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

    //EXPECT_LE(utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_avx2, src.get(), refSpecial.get(), pitch),
    //          utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_px,   src.get(), refSpecial.get(), pitch));

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

    //EXPECT_LE(utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_sse, src.get(), refSpecial.get(), pitch),
    //          utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_px,  src.get(), refSpecial.get(), pitch));

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

    //EXPECT_LE(utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_ssse3, src.get(), refSpecial.get(), pitch),
    //          utils::GetMinTicks(1000, MFX_HEVC_PP::SAD_32x32_px,    src.get(), refSpecial.get(), pitch));

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
    
    Ipp32s shift_08b = 0;
    Ipp32s shift_10b = 2*2;

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

#ifdef PRINT_TICKS
    for (auto wh: dims) {
        Ipp64u ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_avx2<Ipp8u>, src1.get(), pitch1, src2.get(), pitch2, wh[0], wh[1], shift_08b);
        Ipp64u ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_px<Ipp8u>,   src1.get(), pitch1, src2.get(), pitch2, wh[0], wh[1], shift_08b);
        printf("%2dx%2d  8bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);

        ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_avx2<Ipp16u>, src1_10b.get(), pitch1, src2_10b.get(), pitch2, wh[0], wh[1], shift_10b);
        ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_px<Ipp16u>,   src1_10b.get(), pitch1, src2_10b.get(), pitch2, wh[0], wh[1], shift_10b);
        printf("%2dx%2d 10bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);
    }
#endif // PRINT_TICKS

    //EXPECT_LE(utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_avx2<Ipp8u>, src1.get(), pitch1, src2.get(), pitch2, 16, 16),
    //          utils::GetMinTicks(10000, MFX_HEVC_PP::h265_SSE_px<Ipp8u>,   src1.get(), pitch1, src2.get(), pitch2, 16, 16));

    for (auto wh: dims) {
        std::ostringstream buf;
        buf << "Testing " << wh[0] << "x" << wh[1] << " 8bit";
        SCOPED_TRACE(buf.str().c_str());
        EXPECT_EQ(MFX_HEVC_PP::h265_SSE_avx2(src1.get(), pitch1, src2.get(), pitch2, wh[0], wh[1], shift_08b),
                  MFX_HEVC_PP::h265_SSE_px  (src1.get(), pitch1, src2.get(), pitch2, wh[0], wh[1], shift_08b));
    }

    for (auto wh: dims) {
        std::ostringstream buf;
        buf << "Testing " << wh[0] << "x" << wh[1] << " 10bit";
        SCOPED_TRACE(buf.str().c_str());
        EXPECT_EQ(MFX_HEVC_PP::h265_SSE_avx2(src1_10b.get(), pitch1, src2_10b.get(), pitch2, wh[0], wh[1], shift_10b),
                  MFX_HEVC_PP::h265_SSE_px  (src1_10b.get(), pitch1, src2_10b.get(), pitch2, wh[0], wh[1], shift_10b));
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

#ifdef PRINT_TICKS
    for (auto wh: dims) {
        Ipp64u ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_avx2<Ipp8u>, src.get(), pitchSrc, pred.get(), pitchPred, diff1Tst.get(), wh[0], diff2Tst.get(), wh[0], wh[0], wh[1]);
        Ipp64u ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_px<Ipp8u>,   src.get(), pitchSrc, pred.get(), pitchPred, diff1Tst.get(), wh[0], diff2Ref.get(), wh[0], wh[0], wh[1]);
        printf("%2dx%2d  8bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);

        ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_avx2<Ipp16u>, src_10b.get(), pitchSrc, pred_10b.get(), pitchPred, diff1Tst.get(), wh[0], diff2Tst.get(), wh[0], wh[0], wh[1]);
        ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffNv12_px<Ipp16u>,   src_10b.get(), pitchSrc, pred_10b.get(), pitchPred, diff1Ref.get(), wh[0], diff2Ref.get(), wh[0], wh[0], wh[1]);
        printf("%2dx%2d 10bit speedup = %lld / %lld = %.2f\n", wh[0], wh[1],  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);
    }
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


TEST(optimization, AddClipNv12UV_avx2) {
    Ipp32s pitch = 256;
    Ipp32s maxsize = 64;

    auto residU   = utils::MakeAlignedPtr<Ipp16s>(pitch * maxsize,   utils::AlignAvx2);
    auto residV   = utils::MakeAlignedPtr<Ipp16s>(pitch * maxsize,   utils::AlignAvx2);
    auto pred     = utils::MakeAlignedPtr<Ipp8u> (pitch * maxsize*2, utils::AlignAvx2);
    auto dst_px   = utils::MakeAlignedPtr<Ipp8u> (pitch * maxsize*2, utils::AlignAvx2);
    auto dst_avx2 = utils::MakeAlignedPtr<Ipp8u> (pitch * maxsize*2, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (Ipp32s size = 4; size <= 64; size<<=1) {
        utils::InitRandomBlock(rand, residU.get(), pitch, pitch, maxsize, -255, 255);
        utils::InitRandomBlock(rand, residV.get(), pitch, pitch, maxsize, -255, 255);
        utils::InitRandomBlock(rand, pred.get(),   pitch, pitch, maxsize*2, 0, 255);
        utils::InitRandomBlock(rand, dst_px.get(), pitch, pitch, maxsize*2, 0, 255);
        memcpy(dst_avx2.get(), dst_px.get(), sizeof(Ipp8u) * pitch * maxsize*2);

        std::ostringstream buf;
        buf << "Testing " << size << "x" << size << " 8bit";
        SCOPED_TRACE(buf.str().c_str());
        MFX_HEVC_PP::h265_AddClipNv12UV_8u_px  (dst_px.get(),   pitch, pred.get(), pitch, residU.get(), residV.get(), pitch, size);
        MFX_HEVC_PP::h265_AddClipNv12UV_8u_avx2(dst_avx2.get(), pitch, pred.get(), pitch, residU.get(), residV.get(), pitch, size);
        ASSERT_EQ(0, memcmp(dst_avx2.get(), dst_px.get(), sizeof(Ipp8u) * pitch * maxsize*2));
    }

#ifdef PRINT_TICKS
    for (Ipp32s size = 4; size <= 64; size<<=1) {
        Ipp64u ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_AddClipNv12UV_8u_avx2, dst_avx2.get(), pitch, pred.get(), pitch, residU.get(), residV.get(), pitch, size);
        Ipp64u ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_AddClipNv12UV_8u_px,   dst_px.get(),   pitch, pred.get(), pitch, residU.get(), residV.get(), pitch, size);
        printf("%2dx%-2d speedup = %lld / %lld = %.2f\n", size, size,  ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);
    }
    for (Ipp32s size = 4; size <= 64; size<<=1) {
        Ipp64u ticksSse  = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_AddClipNv12UV_8u_sse,  dst_avx2.get(), pitch, pred.get(), pitch, residU.get(), residV.get(), pitch, size);
        Ipp64u ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_AddClipNv12UV_8u_px,   dst_px.get(),   pitch, pred.get(), pitch, residU.get(), residV.get(), pitch, size);
        printf("%2dx%-2d SSE speedup = %lld / %lld = %.2f\n", size, size,  ticksPx, ticksSse, (double)ticksPx / ticksSse);
    }
#endif //PRINT_TICKS
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

// NOTE - because this is a floating point function, changes in compiler settings/version may cause the PX output to vary slightly (no effect on quality)
// e.g. Win32 vs. Win64  - order of float operations in compiled code affects rounding
// if mismatch is observed, check whether PX output is different than before - SSE/AVX2 versions should be identical every run
TEST(optimization, AnalyzeGradient_sse4) {
    Ipp32s width    = 1280;
    Ipp32s height   =  720;
    Ipp32s padding  =   16;
    Ipp32s pitchSrc = (padding + width + padding);
    Ipp32s histMax  =   40;

    auto ySrc     = utils::MakeAlignedPtr<unsigned char>(pitchSrc * (padding + height + padding), utils::AlignSse4);
    auto out4_ref = utils::MakeAlignedPtr<unsigned short>(width * height * histMax / 16, utils::AlignSse4);    // 4x4 histogram
    auto out8_ref = utils::MakeAlignedPtr<unsigned short>(width * height * histMax / 64, utils::AlignSse4);    // 8x8 histogram
    auto out4_sse = utils::MakeAlignedPtr<unsigned short>(width * height * histMax / 16, utils::AlignSse4);
    auto out8_sse = utils::MakeAlignedPtr<unsigned short>(width * height * histMax / 64, utils::AlignSse4);

    auto ySrc_10b     = utils::MakeAlignedPtr<unsigned short>(pitchSrc * (padding + height + padding), utils::AlignSse4);
    auto out4_ref_10b = utils::MakeAlignedPtr<unsigned int>(width * height * histMax / 16, utils::AlignSse4);
    auto out8_ref_10b = utils::MakeAlignedPtr<unsigned int>(width * height * histMax / 64, utils::AlignSse4);
    auto out4_sse_10b = utils::MakeAlignedPtr<unsigned int>(width * height * histMax / 16, utils::AlignSse4);
    auto out8_sse_10b = utils::MakeAlignedPtr<unsigned int>(width * height * histMax / 64, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);
    utils::InitRandomBlock(rand, ySrc.get(), pitchSrc, pitchSrc, (padding + height + padding), 0, 255);
    utils::InitRandomBlock(rand, ySrc_10b.get(), pitchSrc, pitchSrc, (padding + height + padding), 0, 1023);

    // test 8-bit
    MFX_HEVC_PP::h265_AnalyzeGradient_px (ySrc.get() + pitchSrc*padding + padding, pitchSrc, out4_ref.get(), out8_ref.get(), width, height);
    MFX_HEVC_PP::h265_AnalyzeGradient_sse(ySrc.get() + pitchSrc*padding + padding, pitchSrc, out4_sse.get(), out8_sse.get(), width, height);
    EXPECT_EQ(0, memcmp(out4_ref.get(), out4_sse.get(), sizeof(*out4_ref.get()) * width * height * histMax / 16));
    EXPECT_EQ(0, memcmp(out8_ref.get(), out8_sse.get(), sizeof(*out8_ref.get()) * width * height * histMax / 64));

    // test 10-bit
    MFX_HEVC_PP::h265_AnalyzeGradient_px (ySrc_10b.get() + pitchSrc*padding + padding, pitchSrc, out4_ref_10b.get(), out8_ref_10b.get(), width, height);
    MFX_HEVC_PP::h265_AnalyzeGradient_sse(ySrc_10b.get() + pitchSrc*padding + padding, pitchSrc, out4_sse_10b.get(), out8_sse_10b.get(), width, height);
    EXPECT_EQ(0, memcmp(out4_ref_10b.get(), out4_sse_10b.get(), sizeof(*out4_ref_10b.get()) * width * height * histMax / 16));
    EXPECT_EQ(0, memcmp(out8_ref_10b.get(), out8_sse_10b.get(), sizeof(*out8_ref_10b.get()) * width * height * histMax / 64));
}

TEST(optimization, AnalyzeGradient_avx2) {
    Ipp32s width    = 1280;
    Ipp32s height   =  720;
    Ipp32s padding  =   16;
    Ipp32s pitchSrc = (padding + width + padding);
    Ipp32s histMax  =   40;

    auto ySrc      = utils::MakeAlignedPtr<unsigned char>(pitchSrc * (padding + height + padding), utils::AlignAvx2);
    auto out4_ref  = utils::MakeAlignedPtr<unsigned short>(width * height * histMax / 16, utils::AlignAvx2);    // 4x4 histogram
    auto out8_ref  = utils::MakeAlignedPtr<unsigned short>(width * height * histMax / 64, utils::AlignAvx2);    // 8x8 histogram
    auto out4_avx2 = utils::MakeAlignedPtr<unsigned short>(width * height * histMax / 16, utils::AlignAvx2);
    auto out8_avx2 = utils::MakeAlignedPtr<unsigned short>(width * height * histMax / 64, utils::AlignAvx2);

    auto ySrc_10b      = utils::MakeAlignedPtr<unsigned short>(pitchSrc * (padding + height + padding), utils::AlignAvx2);
    auto out4_ref_10b  = utils::MakeAlignedPtr<unsigned int>(width * height * histMax / 16, utils::AlignAvx2);
    auto out8_ref_10b  = utils::MakeAlignedPtr<unsigned int>(width * height * histMax / 64, utils::AlignAvx2);
    auto out4_avx2_10b = utils::MakeAlignedPtr<unsigned int>(width * height * histMax / 16, utils::AlignAvx2);
    auto out8_avx2_10b = utils::MakeAlignedPtr<unsigned int>(width * height * histMax / 64, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);
    utils::InitRandomBlock(rand, ySrc.get(), pitchSrc, pitchSrc, (padding + height + padding), 0, 255);
    utils::InitRandomBlock(rand, ySrc_10b.get(), pitchSrc, pitchSrc, (padding + height + padding), 0, 1023);

    // test 8-bit
    MFX_HEVC_PP::h265_AnalyzeGradient_px  (ySrc.get() + pitchSrc*padding + padding, pitchSrc, out4_ref.get(),  out8_ref.get(),  width, height);
    MFX_HEVC_PP::h265_AnalyzeGradient_avx2(ySrc.get() + pitchSrc*padding + padding, pitchSrc, out4_avx2.get(), out8_avx2.get(), width, height);
    EXPECT_EQ(0, memcmp(out4_ref.get(), out4_avx2.get(), sizeof(*out4_ref.get()) * width * height * histMax / 16));
    EXPECT_EQ(0, memcmp(out8_ref.get(), out8_avx2.get(), sizeof(*out8_ref.get()) * width * height * histMax / 64));

    // test 10-bit
    MFX_HEVC_PP::h265_AnalyzeGradient_px  (ySrc_10b.get() + pitchSrc*padding + padding, pitchSrc, out4_ref_10b.get(),  out8_ref_10b.get(),  width, height);
    MFX_HEVC_PP::h265_AnalyzeGradient_avx2(ySrc_10b.get() + pitchSrc*padding + padding, pitchSrc, out4_avx2_10b.get(), out8_avx2_10b.get(), width, height);
    EXPECT_EQ(0, memcmp(out4_ref_10b.get(), out4_avx2_10b.get(), sizeof(*out4_ref_10b.get()) * width * height * histMax / 16));
    EXPECT_EQ(0, memcmp(out8_ref_10b.get(), out8_avx2_10b.get(), sizeof(*out8_ref_10b.get()) * width * height * histMax / 64));
}

TEST(optimization, DCT_sse4) {

    const int srcSize = 32*32;  // max 32x32
    const int dstSize = 64*64;  // alloc more space to confirm no writing outside of dst block
    int bitDepth;

    auto src     = utils::MakeAlignedPtr<short>(srcSize, utils::AlignSse4);       // transforms assume SSE/AVX2 aligned inputs
    auto dst_px  = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);
    auto dst_sse = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (bitDepth = 8; bitDepth <= 10; bitDepth++) {
        utils::InitRandomBlock(rand, src.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);     
        utils::InitRandomBlock(rand, dst_px.get(), 64, 64, 64, 0, (1 << bitDepth) - 1);
        memcpy(dst_sse.get(), dst_px.get(), sizeof(*dst_px.get())*dstSize);

        // DST 4x4
        MFX_HEVC_PP::h265_DST4x4Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DST4x4Fwd_16s_sse (src.get(), 32, dst_sse.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_sse.get(), sizeof(*dst_px.get()) * dstSize));

        // DCT 4x4
        MFX_HEVC_PP::h265_DCT4x4Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DCT4x4Fwd_16s_sse (src.get(), 32, dst_sse.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_sse.get(), sizeof(*dst_px.get()) * dstSize));

        // DCT 8x8
        MFX_HEVC_PP::h265_DCT8x8Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DCT8x8Fwd_16s_sse (src.get(), 32, dst_sse.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_sse.get(), sizeof(*dst_px.get()) * dstSize));

        // DCT 16x16
        MFX_HEVC_PP::h265_DCT16x16Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DCT16x16Fwd_16s_sse (src.get(), 32, dst_sse.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_sse.get(), sizeof(*dst_px.get()) * dstSize));

        // DCT 32x32
        MFX_HEVC_PP::h265_DCT32x32Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DCT32x32Fwd_16s_sse (src.get(), 32, dst_sse.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_sse.get(), sizeof(*dst_px.get()) * dstSize));
    }
}

TEST(optimization, DCT_avx2) {

    const int srcSize = 32*32;  // max 32x32
    const int dstSize = 64*64;  // alloc more space to confirm no writing outside of dst block
    int bitDepth;

    auto src      = utils::MakeAlignedPtr<short>(srcSize, utils::AlignAvx2);       // transforms assume SSE/AVX2 aligned inputs
    auto dst_px   = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);
    auto dst_avx2 = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (bitDepth = 8; bitDepth <= 10; bitDepth++) {
        utils::InitRandomBlock(rand, src.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);     
        utils::InitRandomBlock(rand, dst_px.get(), 64, 64, 64, 0, (1 << bitDepth) - 1);
        memcpy(dst_avx2.get(), dst_px.get(), sizeof(*dst_px.get())*dstSize);

        // DST 4x4
        MFX_HEVC_PP::h265_DST4x4Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DST4x4Fwd_16s_avx2 (src.get(), 32, dst_avx2.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_avx2.get(), sizeof(*dst_px.get()) * dstSize));

        // DCT 4x4
        MFX_HEVC_PP::h265_DCT4x4Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DCT4x4Fwd_16s_avx2 (src.get(), 32, dst_avx2.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_avx2.get(), sizeof(*dst_px.get()) * dstSize));

        // DCT 8x8
        MFX_HEVC_PP::h265_DCT8x8Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DCT8x8Fwd_16s_avx2 (src.get(), 32, dst_avx2.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_avx2.get(), sizeof(*dst_px.get()) * dstSize));

        // DCT 16x16
        MFX_HEVC_PP::h265_DCT16x16Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DCT16x16Fwd_16s_avx2 (src.get(), 32, dst_avx2.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_avx2.get(), sizeof(*dst_px.get()) * dstSize));

        // DCT 32x32
        MFX_HEVC_PP::h265_DCT32x32Fwd_16s_px  (src.get(), 32, dst_px.get(),  bitDepth);
        MFX_HEVC_PP::h265_DCT32x32Fwd_16s_avx2 (src.get(), 32, dst_avx2.get(), bitDepth);
        EXPECT_EQ(0, memcmp(dst_px.get(), dst_avx2.get(), sizeof(*dst_px.get()) * dstSize));
    }
}

TEST(optimization, IDCT_sse4)
{
    const int srcSize = 32*32;  // max 32x32
    const int predSize = 32*32; // max 32x32
    const int dstSize = 64*64;  // alloc more space to confirm no writing outside of dst block
    int bitDepth;

    // src = Ipp16s
    // dst = Ipp8u or Ipp16u

    auto src       = utils::MakeAlignedPtr<short>(srcSize, utils::AlignSse4);
    auto pred08    = utils::MakeAlignedPtr<unsigned char>(predSize, utils::AlignAvx2);
    auto pred16    = utils::MakeAlignedPtr<unsigned short>(predSize, utils::AlignAvx2);
    auto dst08_px  = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignSse4);
    auto dst08_sse = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignSse4);
    auto dst16_px  = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignSse4);
    auto dst16_sse = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    // inplace = 0 (don't add residual)
    for (bitDepth = 8; bitDepth <= 12; bitDepth++) {
        utils::InitRandomBlock(rand, src.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);     
        utils::InitRandomBlock(rand, dst16_px.get(), 64, 64, 64, 0, (1 << bitDepth) - 1);
        memcpy(dst16_sse.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

        // IDST 4x4
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_px (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_sse(NULL, 0, dst16_sse.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 4x4
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_px (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_sse(NULL, 0, dst16_sse.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 8x8
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_px  (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_sse (NULL, 0, dst16_sse.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 16x16
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_px  (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_sse (NULL, 0, dst16_sse.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 32x32
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_px  (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_sse (NULL, 0, dst16_sse.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));
    }

    // inplace = 1 (add residual, clip to 8 bits)
    for (bitDepth = 8; bitDepth <= 8; bitDepth++) {
        utils::InitRandomBlock(rand, src.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);     
        utils::InitRandomBlock(rand, dst08_px.get(), 64, 64, 64, 0, (1 << bitDepth) - 1);
        utils::InitRandomBlock(rand, pred08.get(), 32, 32, 32, 0, 255);
        memcpy(dst08_sse.get(), dst08_px.get(), sizeof(*dst08_px.get())*dstSize);

        // IDST 4x4
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_px (pred08.get(), 32, dst08_px.get(),   src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_sse(pred08.get(), 32, dst08_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_px.get()) * dstSize));

        // IDCT 4x4
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_px (pred08.get(), 32, dst08_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_sse(pred08.get(), 32, dst08_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_px.get()) * dstSize));

        // IDCT 8x8
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_px  (pred08.get(), 32, dst08_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_sse (pred08.get(), 32, dst08_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_px.get()) * dstSize));

        // IDCT 16x16
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_px  (pred08.get(), 32, dst08_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_sse (pred08.get(), 32, dst08_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_px.get()) * dstSize));

        // IDCT 32x32
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_px  (pred08.get(), 32, dst08_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_sse (pred08.get(), 32, dst08_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_px.get()) * dstSize));
    }

    // inplace = 2 (add residual, clip to > 8 bits)
    for (bitDepth = 9; bitDepth <= 12; bitDepth++) {
        utils::InitRandomBlock(rand, src.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);     
        utils::InitRandomBlock(rand, dst16_px.get(), 64, 64, 64, 0, (1 << bitDepth) - 1);
        utils::InitRandomBlock(rand, pred16.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);
        memcpy(dst16_sse.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

        // IDST 4x4
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),   src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_sse (pred16.get(), 32, dst16_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 4x4
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_sse (pred16.get(), 32, dst16_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 8x8
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_sse (pred16.get(), 32, dst16_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 16x16
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_sse (pred16.get(), 32, dst16_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 32x32
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_sse (pred16.get(), 32, dst16_sse.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));
    }
}

TEST(optimization, DISABLED_IDCT_avx2)
{
    const int srcSize = 32*32;  // max 32x32
    const int predSize = 32*32;  // max 32x32
    const int dstSize = 64*64;  // alloc more space to confirm no writing outside of dst block
    int bitDepth;

    // src = Ipp16s
    // dst = Ipp8u or Ipp16u

    auto src       = utils::MakeAlignedPtr<short>(srcSize, utils::AlignAvx2);
    auto pred08    = utils::MakeAlignedPtr<unsigned char>(predSize, utils::AlignAvx2);
    auto pred16    = utils::MakeAlignedPtr<unsigned short>(predSize, utils::AlignAvx2);
    auto dst08_px  = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
    auto dst08_avx2 = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
    auto dst16_px  = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignAvx2);
    auto dst16_avx2 = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    // inplace = 0 (don't add residual)
    for (bitDepth = 8; bitDepth <= 12; bitDepth++) {
        utils::InitRandomBlock(rand, src.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);     
        utils::InitRandomBlock(rand, dst16_px.get(), 64, 64, 64, 0, (1 << bitDepth) - 1);
        memcpy(dst16_avx2.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

        // IDST 4x4
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_px   (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_avx2 (NULL, 0, dst16_avx2.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 4x4
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_px  (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_avx2(NULL, 0, dst16_avx2.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 8x8
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_px  (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_avx2(NULL, 0, dst16_avx2.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 16x16
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_px  (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_avx2(NULL, 0, dst16_avx2.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 32x32
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_px  (NULL, 0, dst16_px.get(),  src.get(), 64, 0, bitDepth);
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_avx2(NULL, 0, dst16_avx2.get(), src.get(), 64, 0, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));
    }

    // inplace = 1 (add residual, clip to 8 bits)
    for (bitDepth = 8; bitDepth <= 8; bitDepth++) {
        utils::InitRandomBlock(rand, src.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);
        utils::InitRandomBlock(rand, dst08_px.get(), 64, 64, 64, 0, (1 << bitDepth) - 1);
        utils::InitRandomBlock(rand, pred08.get(), 32, 32, 32, 0, 255);
        memcpy(dst08_avx2.get(), dst08_px.get(), sizeof(*dst08_px.get())*dstSize);

        // IDST 4x4
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_px  (pred08.get(), 32, dst08_px.get(),   src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_avx2(pred08.get(), 32, dst08_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_px.get()) * dstSize));

        // IDCT 4x4
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_px  (pred08.get(), 32, dst08_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_avx2(pred08.get(), 32, dst08_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_px.get()) * dstSize));

        // IDCT 8x8
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_px  (pred08.get(), 32, dst08_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_avx2(pred08.get(), 32, dst08_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_px.get()) * dstSize));

        // IDCT 16x16
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_px  (pred08.get(), 32, dst08_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_avx2(pred08.get(), 32, dst08_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_px.get()) * dstSize));

        // IDCT 32x32
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_px  (pred08.get(), 32, dst08_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_avx2(pred08.get(), 32, dst08_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_px.get()) * dstSize));
    }

    // inplace = 2 (add residual, clip to > 8 bits)
    for (bitDepth = 9; bitDepth <= 12; bitDepth++) {
        utils::InitRandomBlock(rand, src.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);     
        utils::InitRandomBlock(rand, dst16_px.get(), 64, 64, 64, 0, (1 << bitDepth) - 1);
        utils::InitRandomBlock(rand, pred16.get(), 32, 32, 32, 0, (1 << bitDepth) - 1);
        memcpy(dst16_avx2.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

        // IDST 4x4
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),   src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DST4x4Inv_16sT_avx2(pred16.get(), 32, dst16_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 4x4
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT4x4Inv_16sT_avx2(pred16.get(), 32, dst16_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 8x8
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT8x8Inv_16sT_avx2(pred16.get(), 32, dst16_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 16x16
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT16x16Inv_16sT_avx2(pred16.get(), 32, dst16_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

        // IDCT 32x32
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_px  (pred16.get(), 32, dst16_px.get(),  src.get(), 64, 1, bitDepth);
        MFX_HEVC_PP::h265_DCT32x32Inv_16sT_avx2(pred16.get(), 32, dst16_avx2.get(), src.get(), 64, 1, bitDepth);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));
    }
}

TEST(optimization, InterpolationLuma_sse4)
{
    const int pitch = 96;
    const int srcSize = pitch*pitch;
    const int dstSize = pitch*pitch;
    int bitDepth, w, h, tabIdx;

    auto src08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignSse4);
    auto src16 = utils::MakeAlignedPtr<short>(srcSize, utils::AlignSse4);
    
    auto dst16_px  = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);
    auto dst16_sse = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, src08.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, src16.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);

    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_sse.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    for (h = 4; h < 64; h += 4) {
        for (w = 4; w < 64; w += 4) {
            for (tabIdx = 1; tabIdx <= 3; tabIdx++) {
                // luma - H - src 8, dst 16
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - V - src 8, dst 16
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - H - src 16, dst 16
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - V - src 16, dst 16
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 10, 512);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 10, 512);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 11, 1024);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 11, 1024);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 12, 2048);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 12, 2048);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));
            }
        }
    }
}

TEST(optimization, InterpolationLuma_avx2)
{
    const int pitch = 96;
    const int srcSize = pitch*pitch;
    const int dstSize = pitch*pitch;
    int bitDepth, w, h, tabIdx;

    auto src08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
    auto src16 = utils::MakeAlignedPtr<short>(srcSize, utils::AlignAvx2);
    
    auto dst16_px   = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);
    auto dst16_avx2 = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, src08.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, src16.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);

    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_avx2.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    for (h = 4; h < 64; h += 4) {
        for (w = 4; w < 64; w += 4) {
            for (tabIdx = 1; tabIdx <= 3; tabIdx++) {
                // luma - H - src 8, dst 16
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_H_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - V - src 8, dst 16
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLuma_s8_d16_V_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - H - src 16, dst 16
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - V - src 16, dst 16
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 0);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 10, 512);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 10, 512);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 11, 1024);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 11, 1024);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 12, 2048);
                MFX_HEVC_PP::h265_InterpLuma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 12, 2048);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));
            }
        }
    }
}

TEST(optimization, InterpolationChroma_sse4)
{
    const int pitch = 96;
    const int srcSize = pitch*pitch;
    const int dstSize = pitch*pitch;
    int bitDepth, w, h, tabIdx, plane;

    auto src08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignSse4);
    auto src16 = utils::MakeAlignedPtr<short>(srcSize, utils::AlignSse4);
    
    auto dst16_px  = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);
    auto dst16_sse = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, src08.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, src16.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);

    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_sse.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    for (plane = 1; plane <= 2; plane++) {    // test interleaved and non-interleaved UV (horiz only)

    for (h = 4; h < 32; h += 2) {
        for (w = 4; w < 32; w += 2) {
            for (tabIdx = 1; tabIdx <= 7; tabIdx++) {
                // chroma - H - src 8, dst 16
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0, plane);
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32, plane);
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                // chroma - V - src 8, dst 16
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                // chroma - H - src 16, dst 16
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0, plane);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0, plane);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 1, 0, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0, plane);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 2, 0, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32, plane);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                // chroma - V - src 16, dst 16
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 0);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 10, 512);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 10, 512);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 11, 1024);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 11, 1024);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 12, 2048);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 12, 2048);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));
            }
        }
    }

    }   // plane
}

TEST(optimization, InterpolationChroma_avx2)
{
    const int pitch = 96;
    const int srcSize = pitch*pitch;
    const int dstSize = pitch*pitch;
    int bitDepth, w, h, tabIdx, plane;

    auto src08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
    auto src16 = utils::MakeAlignedPtr<short>(srcSize, utils::AlignAvx2);
    
    auto dst16_px   = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);
    auto dst16_avx2 = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, src08.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, src16.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);

    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_avx2.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    for (plane = 1; plane <= 2; plane++) {    // test interleaved and non-interleaved UV (horiz only)

    for (h = 4; h < 32; h += 2) {
        for (w = 4; w < 32; w += 2) {
            for (tabIdx = 1; tabIdx <= 7; tabIdx++) {
                // chroma - H - src 8, dst 16
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0, plane);
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32, plane);
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_H_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                // chroma - V - src 8, dst 16
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpChroma_s8_d16_V_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                // chroma - H - src 16, dst 16
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0, plane);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0, plane);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 1, 0, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0, plane);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 2, 0, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32, plane);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32, plane);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                // chroma - V - src 16, dst 16
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 0);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 10, 512);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 10, 512);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 11, 1024);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 11, 1024);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 12, 2048);
                MFX_HEVC_PP::h265_InterpChroma_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 12, 2048);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));
            }
        }
    }

    }   // plane
}

TEST(optimization, InterpolationLumaFast_sse4)
{
    const int pitch = 96;
    const int srcSize = pitch*pitch;
    const int dstSize = pitch*pitch;
    int bitDepth, w, h, tabIdx;

    auto src08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignSse4);
    auto src16 = utils::MakeAlignedPtr<short>(srcSize, utils::AlignSse4);
    
    auto dst16_px  = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);
    auto dst16_sse = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, src08.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, src16.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);

    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_sse.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    for (h = 4; h < 64; h += 4) {
        for (w = 4; w < 64; w += 4) {
            for (tabIdx = 1; tabIdx <= 3; tabIdx++) {
                // luma - H - src 8, dst 16
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - V - src 8, dst 16
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_sse(src08.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - H - src 16, dst 16
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - V - src 16, dst 16
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 10, 512);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 10, 512);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 11, 1024);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 11, 1024);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 12, 2048);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_sse(src16.get() + 8*pitch + 8, pitch, dst16_sse.get(), pitch, tabIdx, w, h, 12, 2048);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_px.get()) * dstSize));
            }
        }
    }
}

TEST(optimization, InterpolationLumaFast_avx2)
{
    const int pitch = 96;
    const int srcSize = pitch*pitch;
    const int dstSize = pitch*pitch;
    int bitDepth, w, h, tabIdx;

    auto src08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
    auto src16 = utils::MakeAlignedPtr<short>(srcSize, utils::AlignAvx2);
    
    auto dst16_px   = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);
    auto dst16_avx2 = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, src08.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, src16.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);

    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_avx2.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    for (h = 4; h < 64; h += 4) {
        for (w = 4; w < 64; w += 4) {
            for (tabIdx = 1; tabIdx <= 3; tabIdx++) {
                // luma - H - src 8, dst 16
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_H_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - V - src 8, dst 16
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_px (src08.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLumaFast_s8_d16_V_avx2(src08.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - H - src 16, dst 16
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_H_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                // luma - V - src 16, dst 16
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 0, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 0, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 1, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 1, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 2, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 2, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 0);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 0);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 6, 32);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 6, 32);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 10, 512);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 10, 512);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 11, 1024);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 11, 1024);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));

                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_px (src16.get() + 8*pitch + 8, pitch, dst16_px.get(),  pitch, tabIdx, w, h, 12, 2048);
                MFX_HEVC_PP::h265_InterpLumaFast_s16_d16_V_avx2(src16.get() + 8*pitch + 8, pitch, dst16_avx2.get(), pitch, tabIdx, w, h, 12, 2048);
                EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_px.get()) * dstSize));
            }
        }
    }
}

TEST(optimization, FilterPredictPels_sse4)
{
    const int srcDstSize = 4*64+32;     // inplace operation
    const int widthTab[4] = {4, 8, 16, 32};
    int width, tabIdx;

    auto predPel08_px  = utils::MakeAlignedPtr<unsigned char>(srcDstSize, utils::AlignSse4);
    auto predPel08_sse = utils::MakeAlignedPtr<unsigned char>(srcDstSize, utils::AlignSse4);

    auto predPel16_px  = utils::MakeAlignedPtr<short>(srcDstSize, utils::AlignSse4);
    auto predPel16_sse = utils::MakeAlignedPtr<short>(srcDstSize, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (tabIdx = 0; tabIdx < 4; tabIdx++) {
        width = widthTab[tabIdx];

        // 8-bit
        utils::InitRandomBlock(rand, predPel08_px.get(), srcDstSize, srcDstSize, 1, 0, (1 << 8) - 1);
        memcpy(predPel08_sse.get(), predPel08_px.get(), sizeof(*predPel08_px.get())*srcDstSize);

        MFX_HEVC_PP::h265_FilterPredictPels_8u_px (predPel08_px.get(), width);
        MFX_HEVC_PP::h265_FilterPredictPels_8u_sse(predPel08_sse.get(), width);
        EXPECT_EQ(0, memcmp(predPel08_px.get(), predPel08_sse.get(), sizeof(*predPel08_sse.get()) * srcDstSize));

        // 10-bit
        utils::InitRandomBlock(rand, predPel16_px.get(), srcDstSize, srcDstSize, 1, 0, (1 << 10) - 1);
        memcpy(predPel16_sse.get(), predPel16_px.get(), sizeof(*predPel16_px.get())*srcDstSize);

        MFX_HEVC_PP::h265_FilterPredictPels_16s_px (predPel16_px.get(), width);
        MFX_HEVC_PP::h265_FilterPredictPels_16s_sse(predPel16_sse.get(), width);
        EXPECT_EQ(0, memcmp(predPel16_px.get(), predPel16_sse.get(), sizeof(*predPel16_sse.get()) * srcDstSize));
    }

    // bilinear filter (only for 32x32)
    utils::InitRandomBlock(rand, predPel08_px.get(), srcDstSize, srcDstSize, 1, 0, (1 << 8) - 1);
    memcpy(predPel08_sse.get(), predPel08_px.get(), sizeof(*predPel08_px.get())*srcDstSize);

    MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_8u_px (predPel08_px.get(),  32, predPel08_px.get()[0*32],  predPel08_px.get()[4*32],  predPel08_px.get()[2*32] );
    MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_8u_sse(predPel08_sse.get(), 32, predPel08_sse.get()[0*32], predPel08_sse.get()[4*32], predPel08_sse.get()[2*32]);
    EXPECT_EQ(0, memcmp(predPel08_px.get(), predPel08_sse.get(), sizeof(*predPel08_sse.get()) * srcDstSize));
    
    utils::InitRandomBlock(rand, predPel16_px.get(), srcDstSize, srcDstSize, 1, 0, (1 << 10) - 1);
    memcpy(predPel16_sse.get(), predPel16_px.get(), sizeof(*predPel16_px.get())*srcDstSize);

    MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_16s_px (predPel16_px.get(),  32, predPel16_px.get()[0*32],  predPel16_px.get()[4*32],  predPel16_px.get()[2*32] );
    MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_16s_sse(predPel16_sse.get(), 32, predPel16_sse.get()[0*32], predPel16_sse.get()[4*32], predPel16_sse.get()[2*32]);
    EXPECT_EQ(0, memcmp(predPel16_px.get(), predPel16_sse.get(), sizeof(*predPel16_sse.get()) * srcDstSize));
}

TEST(optimization, FilterPredictPels_avx2)
{
    const int srcDstSize = 4*64+32;     // inplace operation
    const int widthTab[4] = {4, 8, 16, 32};
    int width, tabIdx;

    auto predPel08_px   = utils::MakeAlignedPtr<unsigned char>(srcDstSize, utils::AlignAvx2);
    auto predPel08_avx2 = utils::MakeAlignedPtr<unsigned char>(srcDstSize, utils::AlignAvx2);

    auto predPel16_px   = utils::MakeAlignedPtr<short>(srcDstSize, utils::AlignAvx2);
    auto predPel16_avx2 = utils::MakeAlignedPtr<short>(srcDstSize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (tabIdx = 0; tabIdx < 4; tabIdx++) {
        width = widthTab[tabIdx];

        // 8-bit
        utils::InitRandomBlock(rand, predPel08_px.get(), srcDstSize, srcDstSize, 1, 0, (1 << 8) - 1);
        memcpy(predPel08_avx2.get(), predPel08_px.get(), sizeof(*predPel08_px.get())*srcDstSize);

        MFX_HEVC_PP::h265_FilterPredictPels_8u_px  (predPel08_px.get(), width);
        MFX_HEVC_PP::h265_FilterPredictPels_8u_avx2(predPel08_avx2.get(), width);
        EXPECT_EQ(0, memcmp(predPel08_px.get(), predPel08_avx2.get(), sizeof(*predPel08_avx2.get()) * srcDstSize));

        // 10-bit
        utils::InitRandomBlock(rand, predPel16_px.get(), srcDstSize, srcDstSize, 1, 0, (1 << 10) - 1);
        memcpy(predPel16_avx2.get(), predPel16_px.get(), sizeof(*predPel16_px.get())*srcDstSize);

        MFX_HEVC_PP::h265_FilterPredictPels_16s_px  (predPel16_px.get(), width);
        MFX_HEVC_PP::h265_FilterPredictPels_16s_avx2(predPel16_avx2.get(), width);
        EXPECT_EQ(0, memcmp(predPel16_px.get(), predPel16_avx2.get(), sizeof(*predPel16_avx2.get()) * srcDstSize));
    }

    // bilinear filter (only for 32x32)
    utils::InitRandomBlock(rand, predPel08_px.get(), srcDstSize, srcDstSize, 1, 0, (1 << 8) - 1);
    memcpy(predPel08_avx2.get(), predPel08_px.get(), sizeof(*predPel08_px.get())*srcDstSize);

    MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_8u_px  (predPel08_px.get(),   32, predPel08_px.get()[0*32],   predPel08_px.get()[4*32],   predPel08_px.get()[2*32]  );
    MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_8u_avx2(predPel08_avx2.get(), 32, predPel08_avx2.get()[0*32], predPel08_avx2.get()[4*32], predPel08_avx2.get()[2*32]);
    EXPECT_EQ(0, memcmp(predPel08_px.get(), predPel08_avx2.get(), sizeof(*predPel08_avx2.get()) * srcDstSize));
    
    utils::InitRandomBlock(rand, predPel16_px.get(), srcDstSize, srcDstSize, 1, 0, (1 << 10) - 1);
    memcpy(predPel16_avx2.get(), predPel16_px.get(), sizeof(*predPel16_px.get())*srcDstSize);

    MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_16s_px  (predPel16_px.get(),   32, predPel16_px.get()[0*32],   predPel16_px.get()[4*32],   predPel16_px.get()[2*32]  );
    MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_16s_avx2(predPel16_avx2.get(), 32, predPel16_avx2.get()[0*32], predPel16_avx2.get()[4*32], predPel16_avx2.get()[2*32]);
    EXPECT_EQ(0, memcmp(predPel16_px.get(), predPel16_avx2.get(), sizeof(*predPel16_avx2.get()) * srcDstSize));
}

TEST(optimization, PredictIntraPlanar_sse4)
{
    const int pitch = 64;
    const int srcSize = (4*64+1)*2;
    const int dstSize = pitch*pitch;

    auto predPel08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignSse4);
    auto dst08_px  = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignSse4);
    auto dst08_sse = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignSse4);

    auto predPel16 = utils::MakeAlignedPtr<short>(srcSize, utils::AlignSse4);
    auto dst16_px  = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);
    auto dst16_sse = utils::MakeAlignedPtr<short>(dstSize, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, predPel08.get(), srcSize, srcSize, 1, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, dst08_px.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    memcpy(dst08_sse.get(), dst08_px.get(), sizeof(*dst08_px.get())*dstSize);

    utils::InitRandomBlock(rand, predPel16.get(), srcSize, srcSize, 1, 0, (1 << 10) - 1);
    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_sse.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    // 4x4
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px (predPel08.get(), dst08_px.get(), pitch, 4);
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_sse(predPel08.get(), dst08_sse.get(), pitch, 4);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px (predPel16.get(), dst16_px.get(), pitch, 4);
    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_sse(predPel16.get(), dst16_sse.get(), pitch, 4);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // 8x8
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px (predPel08.get(), dst08_px.get(), pitch, 8);
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_sse(predPel08.get(), dst08_sse.get(), pitch, 8);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px (predPel16.get(), dst16_px.get(), pitch, 8);
    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_sse(predPel16.get(), dst16_sse.get(), pitch, 8);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // 16x16
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px (predPel08.get(), dst08_px.get(), pitch, 16);
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_sse(predPel08.get(), dst08_sse.get(), pitch, 16);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px (predPel16.get(), dst16_px.get(), pitch, 16);
    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_sse(predPel16.get(), dst16_sse.get(), pitch, 16);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // 32x32
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px (predPel08.get(), dst08_px.get(), pitch, 32);
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_sse(predPel08.get(), dst08_sse.get(), pitch, 32);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px (predPel16.get(), dst16_px.get(), pitch, 32);
    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_sse(predPel16.get(), dst16_sse.get(), pitch, 32);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));
}

TEST(optimization, PredictIntraPlanar_avx2)
{
    const int pitch = 64;
    const int srcSize = (4*64+1)*2;
    const int dstSize = pitch*pitch;

    auto predPel08  = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
    auto dst08_px   = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
    auto dst08_avx2 = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);

    auto predPel16  = utils::MakeAlignedPtr<short>(srcSize, utils::AlignAvx2);
    auto dst16_px   = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);
    auto dst16_avx2 = utils::MakeAlignedPtr<short>(dstSize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, predPel08.get(), srcSize, srcSize, 1, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, dst08_px.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    memcpy(dst08_avx2.get(), dst08_px.get(), sizeof(*dst08_px.get())*dstSize);

    utils::InitRandomBlock(rand, predPel16.get(), srcSize, srcSize, 1, 0, (1 << 10) - 1);
    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_avx2.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    // 4x4
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px (predPel08.get(), dst08_px.get(), pitch, 4);
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_avx2(predPel08.get(), dst08_avx2.get(), pitch, 4);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px (predPel16.get(), dst16_px.get(), pitch, 4);
    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_avx2(predPel16.get(), dst16_avx2.get(), pitch, 4);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // 8x8
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px (predPel08.get(), dst08_px.get(), pitch, 8);
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_avx2(predPel08.get(), dst08_avx2.get(), pitch, 8);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px (predPel16.get(), dst16_px.get(), pitch, 8);
    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_avx2(predPel16.get(), dst16_avx2.get(), pitch, 8);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // 16x16
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px (predPel08.get(), dst08_px.get(), pitch, 16);
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_avx2(predPel08.get(), dst08_avx2.get(), pitch, 16);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px (predPel16.get(), dst16_px.get(), pitch, 16);
    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_avx2(predPel16.get(), dst16_avx2.get(), pitch, 16);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // 32x32
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_px (predPel08.get(), dst08_px.get(), pitch, 32);
    MFX_HEVC_PP::h265_PredictIntra_Planar_8u_avx2(predPel08.get(), dst08_avx2.get(), pitch, 32);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px (predPel16.get(), dst16_px.get(), pitch, 32);
    MFX_HEVC_PP::h265_PredictIntra_Planar_16s_avx2(predPel16.get(), dst16_avx2.get(), pitch, 32);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));
}

TEST(optimization, PredictIntraAngleSingle_sse4)
{
    const int pitch = 64;
    const int srcSize = (4*64+1)*2;
    const int dstSize = pitch*pitch;
    int mode;

    auto predPel08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignSse4);
    auto dst08_px  = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignSse4);
    auto dst08_sse = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignSse4);

    auto predPel16 = utils::MakeAlignedPtr<unsigned short>(srcSize, utils::AlignSse4);
    auto dst16_px  = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignSse4);
    auto dst16_sse = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, predPel08.get(), srcSize, srcSize, 1, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, dst08_px.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    memcpy(dst08_sse.get(), dst08_px.get(), sizeof(*dst08_px.get())*dstSize);

    utils::InitRandomBlock(rand, predPel16.get(), srcSize, srcSize, 1, 0, (1 << 10) - 1);
    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_sse.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    // single angle at a time (with transpose)
    for (mode = 2; mode < 35; mode++) {
        // 4x4
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_px (mode, predPel08.get(), dst08_px.get(), pitch, 4);
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_sse(mode, predPel08.get(), dst08_sse.get(), pitch, 4);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px (mode, predPel16.get(), dst16_px.get(), pitch, 4);
        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_sse(mode, predPel16.get(), dst16_sse.get(), pitch, 4);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

        // 8x8
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_px (mode, predPel08.get(), dst08_px.get(), pitch, 8);
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_sse(mode, predPel08.get(), dst08_sse.get(), pitch, 8);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px (mode, predPel16.get(), dst16_px.get(), pitch, 8);
        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_sse(mode, predPel16.get(), dst16_sse.get(), pitch, 8);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

        // 16x16
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_px (mode, predPel08.get(), dst08_px.get(), pitch, 16);
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_sse(mode, predPel08.get(), dst08_sse.get(), pitch, 16);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px (mode, predPel16.get(), dst16_px.get(), pitch, 16);
        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_sse(mode, predPel16.get(), dst16_sse.get(), pitch, 16);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

        // 32x32
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_px (mode, predPel08.get(), dst08_px.get(), pitch, 32);
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_sse(mode, predPel08.get(), dst08_sse.get(), pitch, 32);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px (mode, predPel16.get(), dst16_px.get(), pitch, 32);
        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_sse(mode, predPel16.get(), dst16_sse.get(), pitch, 32);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));
    }

    // single angle at a time (no transpose)
    for (mode = 2; mode < 35; mode++) {
        // 4x4
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_px (mode, predPel08.get(), dst08_px.get(), pitch, 4);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_sse(mode, predPel08.get(), dst08_sse.get(), pitch, 4);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_px (mode, predPel16.get(), dst16_px.get(), pitch, 4);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_sse(mode, predPel16.get(), dst16_sse.get(), pitch, 4);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

        // 8x8
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_px (mode, predPel08.get(), dst08_px.get(), pitch, 8);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_sse(mode, predPel08.get(), dst08_sse.get(), pitch, 8);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_px (mode, predPel16.get(), dst16_px.get(), pitch, 8);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_sse(mode, predPel16.get(), dst16_sse.get(), pitch, 8);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

        // 16x16
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_px (mode, predPel08.get(), dst08_px.get(), pitch, 16);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_sse(mode, predPel08.get(), dst08_sse.get(), pitch, 16);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_px (mode, predPel16.get(), dst16_px.get(), pitch, 16);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_sse(mode, predPel16.get(), dst16_sse.get(), pitch, 16);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

        // 32x32
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_px (mode, predPel08.get(), dst08_px.get(), pitch, 32);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_sse(mode, predPel08.get(), dst08_sse.get(), pitch, 32);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_px (mode, predPel16.get(), dst16_px.get(), pitch, 32);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_sse(mode, predPel16.get(), dst16_sse.get(), pitch, 32);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));
    }
}

TEST(optimization, PredictIntraAngleSingle_avx2)
{
    const int pitch = 64;
    const int srcSize = (4*64+1)*2;
    const int dstSize = pitch*pitch;
    int mode;

    auto predPel08  = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
    auto dst08_px   = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
    auto dst08_avx2 = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);

    auto predPel16  = utils::MakeAlignedPtr<unsigned short>(srcSize, utils::AlignAvx2);
    auto dst16_px   = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignAvx2);
    auto dst16_avx2 = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, predPel08.get(), srcSize, srcSize, 1, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, dst08_px.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    memcpy(dst08_avx2.get(), dst08_px.get(), sizeof(*dst08_px.get())*dstSize);

    utils::InitRandomBlock(rand, predPel16.get(), srcSize, srcSize, 1, 0, (1 << 10) - 1);
    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_avx2.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    // single angle at a time (with transpose)
    for (mode = 2; mode < 35; mode++) {
        // 4x4
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_px  (mode, predPel08.get(), dst08_px.get(), pitch, 4);
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_avx2(mode, predPel08.get(), dst08_avx2.get(), pitch, 4);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px  (mode, predPel16.get(), dst16_px.get(), pitch, 4);
        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_avx2(mode, predPel16.get(), dst16_avx2.get(), pitch, 4);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

        // 8x8
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_px  (mode, predPel08.get(), dst08_px.get(), pitch, 8);
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_avx2(mode, predPel08.get(), dst08_avx2.get(), pitch, 8);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px  (mode, predPel16.get(), dst16_px.get(), pitch, 8);
        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_avx2(mode, predPel16.get(), dst16_avx2.get(), pitch, 8);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

        // 16x16
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_px  (mode, predPel08.get(), dst08_px.get(), pitch, 16);
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_avx2(mode, predPel08.get(), dst08_avx2.get(), pitch, 16);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px  (mode, predPel16.get(), dst16_px.get(), pitch, 16);
        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_avx2(mode, predPel16.get(), dst16_avx2.get(), pitch, 16);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

        // 32x32
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_px  (mode, predPel08.get(), dst08_px.get(), pitch, 32);
        MFX_HEVC_PP::h265_PredictIntra_Ang_8u_avx2(mode, predPel08.get(), dst08_avx2.get(), pitch, 32);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px  (mode, predPel16.get(), dst16_px.get(), pitch, 32);
        MFX_HEVC_PP::h265_PredictIntra_Ang_16u_avx2(mode, predPel16.get(), dst16_avx2.get(), pitch, 32);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));
    }

    // single angle at a time (no transpose)
    for (mode = 2; mode < 35; mode++) {
        // 4x4
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_px  (mode, predPel08.get(), dst08_px.get(), pitch, 4);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_avx2(mode, predPel08.get(), dst08_avx2.get(), pitch, 4);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_px  (mode, predPel16.get(), dst16_px.get(), pitch, 4);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_avx2(mode, predPel16.get(), dst16_avx2.get(), pitch, 4);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

        // 8x8
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_px  (mode, predPel08.get(), dst08_px.get(), pitch, 8);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_avx2(mode, predPel08.get(), dst08_avx2.get(), pitch, 8);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_px  (mode, predPel16.get(), dst16_px.get(), pitch, 8);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_avx2(mode, predPel16.get(), dst16_avx2.get(), pitch, 8);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

        // 16x16
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_px (mode, predPel08.get(), dst08_px.get(), pitch, 16);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_avx2(mode, predPel08.get(), dst08_avx2.get(), pitch, 16);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_px  (mode, predPel16.get(), dst16_px.get(), pitch, 16);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_avx2(mode, predPel16.get(), dst16_avx2.get(), pitch, 16);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

        // 32x32
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_px  (mode, predPel08.get(), dst08_px.get(), pitch, 32);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_8u_avx2(mode, predPel08.get(), dst08_avx2.get(), pitch, 32);
        EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_px  (mode, predPel16.get(), dst16_px.get(), pitch, 32);
        MFX_HEVC_PP::h265_PredictIntra_Ang_NoTranspose_16u_avx2(mode, predPel16.get(), dst16_avx2.get(), pitch, 32);
        EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));
    }
}

TEST(optimization, PredictIntraAngleMultiple_sse4)
{
    ASSERT_EQ(ippStsNoErr, MFX_HEVC_PP::InitDispatcher(MFX_HEVC_PP::CPU_FEAT_SSE4));

    const int pitch = 64;
    const int srcSize = (4*64+1)*2;
    const int dstSize = 35*pitch*pitch;

    auto predPel08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignSse4);
    auto filtPel08 = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignSse4);
    auto dst08_px  = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignSse4);
    auto dst08_sse = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignSse4);

    auto predPel16 = utils::MakeAlignedPtr<unsigned short>(srcSize, utils::AlignSse4);
    auto filtPel16 = utils::MakeAlignedPtr<unsigned short>(srcSize, utils::AlignSse4);
    auto dst16_px  = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignSse4);
    auto dst16_sse = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignSse4);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, predPel08.get(), srcSize, srcSize, 1, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, filtPel08.get(), srcSize, srcSize, 1, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, dst08_px.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    memcpy(dst08_sse.get(), dst08_px.get(), sizeof(*dst08_px.get())*dstSize);

    utils::InitRandomBlock(rand, predPel16.get(), srcSize, srcSize, 1, 0, (1 << 10) - 1);
    utils::InitRandomBlock(rand, filtPel16.get(), srcSize, srcSize, 1, 0, (1 << 10) - 1);
    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_sse.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    // all even angles [2-34] (output in packed width*width blocks for each mode)
    
    // 4x4
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_px (predPel08.get(), filtPel08.get(), dst08_px.get(), 4);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_sse(predPel08.get(), filtPel08.get(), dst08_sse.get(), 4);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px (predPel16.get(), filtPel16.get(), dst16_px.get(), 4, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_sse(predPel16.get(), filtPel16.get(), dst16_sse.get(), 4, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // 8x8
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_px (predPel08.get(), filtPel08.get(), dst08_px.get(), 8);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_sse(predPel08.get(), filtPel08.get(), dst08_sse.get(), 8);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px (predPel16.get(), filtPel16.get(), dst16_px.get(), 8, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_sse(predPel16.get(), filtPel16.get(), dst16_sse.get(), 8, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // 16x16
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_px (predPel08.get(), filtPel08.get(), dst08_px.get(), 16);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_sse(predPel08.get(), filtPel08.get(), dst08_sse.get(), 16);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px (predPel16.get(), filtPel16.get(), dst16_px.get(), 16, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_sse(predPel16.get(), filtPel16.get(), dst16_sse.get(), 16, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // 32x32
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_px (predPel08.get(), filtPel08.get(), dst08_px.get(), 32);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_sse(predPel08.get(), filtPel08.get(), dst08_sse.get(), 32);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px (predPel16.get(), filtPel16.get(), dst16_px.get(), 32, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_sse(predPel16.get(), filtPel16.get(), dst16_sse.get(), 32, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // all angles [2-34] (output in packed width*width blocks for each mode)

    // 4x4
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_px (predPel08.get(), filtPel08.get(), dst08_px.get(), 4);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_sse(predPel08.get(), filtPel08.get(), dst08_sse.get(), 4);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px (predPel16.get(), filtPel16.get(), dst16_px.get(), 4, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_sse(predPel16.get(), filtPel16.get(), dst16_sse.get(), 4, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // 8x8
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_px (predPel08.get(), filtPel08.get(), dst08_px.get(), 8);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_sse(predPel08.get(), filtPel08.get(), dst08_sse.get(), 8);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px (predPel16.get(), filtPel16.get(), dst16_px.get(), 8, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_sse(predPel16.get(), filtPel16.get(), dst16_sse.get(), 8, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // 16x16
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_px (predPel08.get(), filtPel08.get(), dst08_px.get(), 16);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_sse(predPel08.get(), filtPel08.get(), dst08_sse.get(), 16);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px (predPel16.get(), filtPel16.get(), dst16_px.get(), 16, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_sse(predPel16.get(), filtPel16.get(), dst16_sse.get(), 16, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));

    // 32x32
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_px (predPel08.get(), filtPel08.get(), dst08_px.get(), 32);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_sse(predPel08.get(), filtPel08.get(), dst08_sse.get(), 32);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_sse.get(), sizeof(*dst08_sse.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px (predPel16.get(), filtPel16.get(), dst16_px.get(), 32, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_sse(predPel16.get(), filtPel16.get(), dst16_sse.get(), 32, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_sse.get(), sizeof(*dst16_sse.get()) * dstSize));
}

TEST(optimization, PredictIntraAngleMultiple_avx2)
{
    ASSERT_EQ(ippStsNoErr, MFX_HEVC_PP::InitDispatcher(MFX_HEVC_PP::CPU_FEAT_AVX2));

    const int pitch = 64;
    const int srcSize = (4*64+1)*2;
    const int dstSize = 35*pitch*pitch;

    auto predPel08  = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
    auto filtPel08  = utils::MakeAlignedPtr<unsigned char>(srcSize, utils::AlignAvx2);
    auto dst08_px   = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);
    auto dst08_avx2 = utils::MakeAlignedPtr<unsigned char>(dstSize, utils::AlignAvx2);

    auto predPel16  = utils::MakeAlignedPtr<unsigned short>(srcSize, utils::AlignAvx2);
    auto filtPel16  = utils::MakeAlignedPtr<unsigned short>(srcSize, utils::AlignAvx2);
    auto dst16_px   = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignAvx2);
    auto dst16_avx2 = utils::MakeAlignedPtr<unsigned short>(dstSize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    utils::InitRandomBlock(rand, predPel08.get(), srcSize, srcSize, 1, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, filtPel08.get(), srcSize, srcSize, 1, 0, (1 << 8) - 1);
    utils::InitRandomBlock(rand, dst08_px.get(), pitch, pitch, pitch, 0, (1 << 8) - 1);
    memcpy(dst08_avx2.get(), dst08_px.get(), sizeof(*dst08_px.get())*dstSize);

    utils::InitRandomBlock(rand, predPel16.get(), srcSize, srcSize, 1, 0, (1 << 10) - 1);
    utils::InitRandomBlock(rand, filtPel16.get(), srcSize, srcSize, 1, 0, (1 << 10) - 1);
    utils::InitRandomBlock(rand, dst16_px.get(), pitch, pitch, pitch, 0, (1 << 10) - 1);
    memcpy(dst16_avx2.get(), dst16_px.get(), sizeof(*dst16_px.get())*dstSize);

    // all even angles [2-34] (output in packed width*width blocks for each mode)
    
    // 4x4
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_px  (predPel08.get(), filtPel08.get(), dst08_px.get(), 4);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_avx2(predPel08.get(), filtPel08.get(), dst08_avx2.get(), 4);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px  (predPel16.get(), filtPel16.get(), dst16_px.get(), 4, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_avx2(predPel16.get(), filtPel16.get(), dst16_avx2.get(), 4, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // 8x8
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_px  (predPel08.get(), filtPel08.get(), dst08_px.get(), 8);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_avx2(predPel08.get(), filtPel08.get(), dst08_avx2.get(), 8);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px  (predPel16.get(), filtPel16.get(), dst16_px.get(), 8, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_avx2(predPel16.get(), filtPel16.get(), dst16_avx2.get(), 8, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // 16x16
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_px  (predPel08.get(), filtPel08.get(), dst08_px.get(), 16);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_avx2(predPel08.get(), filtPel08.get(), dst08_avx2.get(), 16);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px  (predPel16.get(), filtPel16.get(), dst16_px.get(), 16, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_avx2(predPel16.get(), filtPel16.get(), dst16_avx2.get(), 16, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // 32x32
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_px  (predPel08.get(), filtPel08.get(), dst08_px.get(), 32);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_8u_avx2(predPel08.get(), filtPel08.get(), dst08_avx2.get(), 32);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_px  (predPel16.get(), filtPel16.get(), dst16_px.get(), 32, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_Even_16u_avx2(predPel16.get(), filtPel16.get(), dst16_avx2.get(), 32, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // all angles [2-34] (output in packed width*width blocks for each mode)

    // 4x4
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_px  (predPel08.get(), filtPel08.get(), dst08_px.get(), 4);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_avx2(predPel08.get(), filtPel08.get(), dst08_avx2.get(), 4);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px  (predPel16.get(), filtPel16.get(), dst16_px.get(), 4, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_avx2(predPel16.get(), filtPel16.get(), dst16_avx2.get(), 4, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // 8x8
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_px  (predPel08.get(), filtPel08.get(), dst08_px.get(), 8);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_avx2(predPel08.get(), filtPel08.get(), dst08_avx2.get(), 8);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px  (predPel16.get(), filtPel16.get(), dst16_px.get(), 8, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_avx2(predPel16.get(), filtPel16.get(), dst16_avx2.get(), 8, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // 16x16
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_px  (predPel08.get(), filtPel08.get(), dst08_px.get(), 16);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_avx2(predPel08.get(), filtPel08.get(), dst08_avx2.get(), 16);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px  (predPel16.get(), filtPel16.get(), dst16_px.get(), 16, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_avx2(predPel16.get(), filtPel16.get(), dst16_avx2.get(), 16, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));

    // 32x32
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_px  (predPel08.get(), filtPel08.get(), dst08_px.get(), 32);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_8u_avx2(predPel08.get(), filtPel08.get(), dst08_avx2.get(), 32);
    EXPECT_EQ(0, memcmp(dst08_px.get(), dst08_avx2.get(), sizeof(*dst08_avx2.get()) * dstSize));

    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_px  (predPel16.get(), filtPel16.get(), dst16_px.get(), 32, 10);
    MFX_HEVC_PP::h265_PredictIntra_Ang_All_16u_avx2(predPel16.get(), filtPel16.get(), dst16_avx2.get(), 32, 10);
    EXPECT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(*dst16_avx2.get()) * dstSize));
}

TEST(optimization, PredictIntra_Planar_ChromaNV12_avx2) {
    const int maxSize  = 32;
    const int dstPitch = 64;
    
    Ipp8u  predPel8[4*maxSize+1];
    Ipp16u predPel16[4*maxSize+1];
    auto dst8_px    = utils::MakeAlignedPtr<Ipp8u>(maxSize * dstPitch, utils::AlignAvx2);
    auto dst8_avx2  = utils::MakeAlignedPtr<Ipp8u>(maxSize * dstPitch, utils::AlignAvx2);
    auto dst16_px   = utils::MakeAlignedPtr<Ipp16u>(maxSize * dstPitch, utils::AlignAvx2);
    auto dst16_avx2 = utils::MakeAlignedPtr<Ipp16u>(maxSize * dstPitch, utils::AlignAvx2);
    memset(dst8_px.get(), 0, sizeof(Ipp8u) * maxSize * dstPitch);
    memset(dst8_avx2.get(), 0, sizeof(Ipp8u) * maxSize * dstPitch);
    memset(dst16_px.get(), 0, sizeof(Ipp16u) * maxSize * dstPitch);
    memset(dst16_avx2.get(), 0, sizeof(Ipp16u) * maxSize * dstPitch);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (Ipp32s bitDepth = 8; bitDepth <= 10; bitDepth++) {
        for (Ipp32s blkSize = 4; blkSize <= 32; blkSize <<= 1) {
            if (bitDepth == 8) {
                utils::InitRandomBlock(rand, predPel8,  4*maxSize+1, 4*maxSize+1, 1, 0, 255);
                MFX_HEVC_PP::h265_PredictIntra_Planar_ChromaNV12_8u_px(predPel8, dst8_px.get(),   dstPitch, blkSize);
                MFX_HEVC_PP::h265_PredictIntra_Planar_ChromaNV12_8u_avx2(predPel8, dst8_avx2.get(), dstPitch, blkSize);
                ASSERT_EQ(0, memcmp(dst8_px.get(), dst8_avx2.get(), sizeof(Ipp8u) * blkSize * dstPitch));

#ifdef PRINT_TICKS
                Ipp64s tpx = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_PredictIntra_Planar_ChromaNV12_8u_px, predPel8, dst8_px.get(),   dstPitch, blkSize);
                Ipp64s tavx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_PredictIntra_Planar_ChromaNV12_8u_avx2, predPel8, dst8_avx2.get(), dstPitch, blkSize);
                printf("%d-bit %2d*%-2d %d vs %d\n", bitDepth, blkSize, blkSize, (Ipp32s)tpx, (Ipp32s)tavx2);
#endif // PRINT_TICKS
            } else {
                utils::InitRandomBlock(rand, predPel16, 4*maxSize+1, 4*maxSize+1, 1, 0, (1 << bitDepth) - 1);
                MFX_HEVC_PP::h265_PredictIntra_Planar_ChromaNV12_16u(predPel16, dst16_px.get(),   dstPitch, blkSize);
                MFX_HEVC_PP::h265_PredictIntra_Planar_ChromaNV12_16u(predPel16, dst16_avx2.get(), dstPitch, blkSize);
                ASSERT_EQ(0, memcmp(dst16_px.get(), dst16_avx2.get(), sizeof(Ipp16u) * blkSize * dstPitch));
            }
        }
    }
}


namespace H265Enc {
    extern const Ipp8u h265_qp_rem[];
    extern const Ipp8u h265_qp6[];
    extern const Ipp8u h265_quant_table_inv[];
    extern const Ipp16u h265_quant_table_fwd[];
};

TEST(optimization, QuantInv_avx2) {
    const int srcSize = 32;
    const int dstSize = 32;
    const int srcPitch = 64;
    const int dstPitch = 64;

    auto src = utils::MakeAlignedPtr<short>(srcSize * srcPitch, utils::AlignAvx2);
    auto dst_px = utils::MakeAlignedPtr<short>(dstSize * dstPitch, utils::AlignAvx2);
    auto dst_avx2 = utils::MakeAlignedPtr<short>(dstSize * dstPitch, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (Ipp32s bitDepth = 8; bitDepth <= 10; bitDepth++) {
        for (Ipp32s log2TrSize = 2; log2TrSize <= 5; log2TrSize++) {
            utils::InitRandomBlock(rand, src.get(), srcPitch, srcSize, srcSize, -0x8000, 0x7fff);
            utils::InitRandomBlock(rand, dst_px.get(), dstPitch, dstSize, dstSize, -0x8000, 0x7fff);
            memcpy(dst_avx2.get(), dst_px.get(), sizeof(Ipp16s) * dstPitch * dstSize);
            for (Ipp32s qp = 0; qp <= 51 + (bitDepth - 8) * 6; qp++) {
                Ipp32s qpRem = H265Enc::h265_qp_rem[qp];
                Ipp32s qp6 = H265Enc::h265_qp6[qp];
                Ipp32s scale = H265Enc::h265_quant_table_inv[qpRem] << qp6;
                Ipp32s shift = bitDepth + log2TrSize - 9;
                Ipp32s offset = 1 << (shift - 1);
                Ipp32s numCoeffs = 1 << (log2TrSize << 1);

                MFX_HEVC_PP::h265_QuantInv_16s_px(src.get(), dst_px.get(), numCoeffs, scale, offset, shift);
                MFX_HEVC_PP::h265_QuantInv_16s_avx2(src.get(), dst_avx2.get(), numCoeffs, scale, offset, shift);
                ASSERT_EQ(0, memcmp(dst_px.get(), dst_avx2.get(), sizeof(Ipp16s) * dstSize * dstPitch));

#ifdef PRINT_TICKS
                if (qp == 30 && bitDepth == 8) {
                    Ipp64s tpx = utils::GetMinTicks(1000000, MFX_HEVC_PP::h265_QuantInv_16s_px, src.get(), dst_px.get(), numCoeffs, scale, offset, shift);
                    Ipp64s tavx2 = utils::GetMinTicks(1000000, MFX_HEVC_PP::h265_QuantInv_16s_avx2, src.get(), dst_avx2.get(), numCoeffs, scale, offset, shift);
                    printf("%d %d %d\n", 1 << log2TrSize, (Ipp32s)tpx, (Ipp32s)tavx2);
                }
#endif // PRINT_TICKS
            }
        }
    }
}


TEST(optimization, QuantFwd_avx2) {
    const int srcSize = 32;
    const int dstSize = 32;
    const int srcPitch = 64;
    const int dstPitch = 64;

    auto src = utils::MakeAlignedPtr<short>(srcSize * srcPitch, utils::AlignAvx2);
    auto dst_px = utils::MakeAlignedPtr<short>(dstSize * dstPitch, utils::AlignAvx2);
    auto dst_avx2 = utils::MakeAlignedPtr<short>(dstSize * dstPitch, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (Ipp32s isIntra = 0; isIntra <= 1; isIntra++) {
        for (Ipp32s bitDepth = 8; bitDepth <= 10; bitDepth++) {
            for (Ipp32s log2TrSize = 2; log2TrSize <= 5; log2TrSize++) {
                utils::InitRandomBlock(rand, src.get(), srcPitch, srcSize, srcSize, -0x8000, 0x7fff);
                utils::InitRandomBlock(rand, dst_px.get(), dstPitch, dstSize, dstSize, -0x8000, 0x7fff);
                memcpy(dst_avx2.get(), dst_px.get(), sizeof(Ipp16s) * dstPitch * dstSize);
                for (Ipp32s qp = 0; qp <= 51 + (bitDepth - 8) * 6; qp++) {
                    Ipp32s qpRem = H265Enc::h265_qp_rem[qp];
                    Ipp32s qp6 = H265Enc::h265_qp6[qp];
                    Ipp32s numCoeffs = 1 << (log2TrSize << 1);
                    Ipp32s scale = 29 + qp6 - bitDepth - log2TrSize;
                    Ipp32s scaleLevel = H265Enc::h265_quant_table_fwd[qpRem];
                    Ipp32s scaleOffset = (isIntra ? 171 : 85) << (scale - 9);
                    MFX_HEVC_PP::h265_QuantFwd_16s_px(src.get(), dst_px.get(), numCoeffs, scaleLevel, scaleOffset, scale);
                    MFX_HEVC_PP::h265_QuantFwd_16s_avx2(src.get(), dst_avx2.get(), numCoeffs, scaleLevel, scaleOffset, scale);
                    ASSERT_EQ(0, memcmp(dst_px.get(), dst_avx2.get(), sizeof(Ipp16s) * dstSize * dstPitch));

#ifdef PRINT_TICKS
                    if (qp == 30 && bitDepth == 8 && isIntra == 0) {
                        Ipp64s tpx = utils::GetMinTicks(1000000, MFX_HEVC_PP::h265_QuantFwd_16s_px, src.get(), dst_px.get(), numCoeffs, scaleLevel, scaleOffset, scale);
                        Ipp64s tavx2 = utils::GetMinTicks(1000000, MFX_HEVC_PP::h265_QuantFwd_16s_avx2, src.get(), dst_avx2.get(), numCoeffs, scaleLevel, scaleOffset, scale);
                        printf("%d %d %d\n", 1 << log2TrSize, (Ipp32s)tpx, (Ipp32s)tavx2);
                    }
#endif // PRINT_TICKS
                }
            }
        }
    }
}


TEST(optimization, Quant_zCost_avx2) {
    const int maxsize = 32;

    auto src = utils::MakeAlignedPtr<short>(maxsize * maxsize, utils::AlignAvx2);
    auto dstCoefs_px   = utils::MakeAlignedPtr<Ipp32u>(maxsize * maxsize, utils::AlignAvx2);
    auto dstCoefs_avx2 = utils::MakeAlignedPtr<Ipp32u>(maxsize * maxsize, utils::AlignAvx2);
    auto dstCosts_px   = utils::MakeAlignedPtr<Ipp64s>(maxsize * maxsize, utils::AlignAvx2);
    auto dstCosts_avx2 = utils::MakeAlignedPtr<Ipp64s>(maxsize * maxsize, utils::AlignAvx2);
    memset(dstCoefs_px.get(),   0, sizeof(Ipp32u) * maxsize * maxsize);
    memset(dstCoefs_avx2.get(), 0, sizeof(Ipp32u) * maxsize * maxsize);
    memset(dstCosts_px.get(),   0, sizeof(Ipp64s) * maxsize * maxsize);
    memset(dstCosts_avx2.get(), 0, sizeof(Ipp64s) * maxsize * maxsize);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (Ipp32s bitDepth = 8; bitDepth <= 10; bitDepth++) {
        for (Ipp32s log2TrSize = 2; log2TrSize <= 5; log2TrSize++) {
            utils::InitRandomBlock(rand, src.get(), maxsize, maxsize, maxsize, -0x8000, 0x7fff);
            for (Ipp32s qp = 0; qp <= 51 + (bitDepth - 8) * 6; qp++) {
                Ipp32s qpRem = H265Enc::h265_qp_rem[qp];
                Ipp32s qp6 = H265Enc::h265_qp6[qp];
                Ipp32s numCoeffs = 1 << (log2TrSize << 1);
                Ipp32s qbits = 29 + qp6 - bitDepth - log2TrSize;
                Ipp32s scale = H265Enc::h265_quant_table_fwd[qpRem];
                Ipp32s qshift = 1 << (qbits - 1);
                MFX_HEVC_PP::h265_Quant_zCost_16s_px  (src.get(), dstCoefs_px.get(),   dstCosts_px.get(),   numCoeffs, scale, qshift, qbits, numCoeffs << 1);
                MFX_HEVC_PP::h265_Quant_zCost_16s_avx2(src.get(), dstCoefs_avx2.get(), dstCosts_avx2.get(), numCoeffs, scale, qshift, qbits, numCoeffs << 1);
                ASSERT_EQ(0, memcmp(dstCoefs_px.get(), dstCoefs_avx2.get(), sizeof(Ipp32u) * maxsize * maxsize));
                ASSERT_EQ(0, memcmp(dstCosts_px.get(), dstCosts_avx2.get(), sizeof(Ipp64s) * maxsize * maxsize));

#ifdef PRINT_TICKS
                if (qp == 30 && bitDepth == 8) {
                    Ipp64s tpx   = utils::GetMinTicks(100000, MFX_HEVC_PP::h265_Quant_zCost_16s_px,   src.get(), dstCoefs_px.get(),   dstCosts_px.get(),   numCoeffs, scale, qshift, qbits, numCoeffs << 1);
                    Ipp64s tavx2 = utils::GetMinTicks(100000, MFX_HEVC_PP::h265_Quant_zCost_16s_avx2, src.get(), dstCoefs_avx2.get(), dstCosts_avx2.get(), numCoeffs, scale, qshift, qbits, numCoeffs << 1);
                    printf("%2d*%-2d: %d %d\n", 1<<log2TrSize, 1<<log2TrSize, (Ipp32s)tpx, (Ipp32s)tavx2);
                }
#endif // PRINT_TICKS
            }
        }
    }
}


TEST(optimization, ComputeRsCs_avx2) {
    const Ipp32s padding = utils::AlignAvx2;
    Ipp32s cusize = 64;
    Ipp32s pitch  = std::max(256, cusize+3*padding);

    Ipp32s rs_px[256] = {}, rs_avx2[256] = {};
    Ipp32s cs_px[256] = {}, cs_avx2[256] = {};

    auto srcBuf = utils::MakeAlignedPtr<unsigned char>(pitch*(cusize+2*padding), utils::AlignAvx2);
    auto src = srcBuf.get() + padding + padding * pitch;

    std::minstd_rand0 rand;
    rand.seed(0x1234);
    utils::InitRandomBlock(rand, srcBuf.get(), pitch, cusize+2*padding, cusize+2*padding, 0, 255);

    MFX_HEVC_PP::h265_ComputeRsCs_px<Ipp8u>(src, pitch, rs_px,   cs_px,   16, cusize, cusize);
    MFX_HEVC_PP::h265_ComputeRsCs_8u_avx2  (src, pitch, rs_avx2, cs_avx2, 16, cusize, cusize);
    ASSERT_EQ(0, memcmp(rs_px, rs_avx2, sizeof(rs_px)));
    ASSERT_EQ(0, memcmp(cs_px, cs_avx2, sizeof(cs_px)));
#ifdef PRINT_TICKS
    Ipp64s tpx   = utils::GetMinTicks(100000, MFX_HEVC_PP::h265_ComputeRsCs_px<Ipp8u>, src, pitch, rs_px,   cs_px,   16, cusize, cusize);
    Ipp64s tsse  = utils::GetMinTicks(100000, MFX_HEVC_PP::h265_ComputeRsCs_8u_sse,    src, pitch, rs_avx2, cs_avx2, 16, cusize, cusize);
    Ipp64s tavx2 = utils::GetMinTicks(100000, MFX_HEVC_PP::h265_ComputeRsCs_8u_avx2,   src, pitch, rs_avx2, cs_avx2, 16, cusize, cusize);
    printf("%d %d %d\n", (Ipp32s)tpx, (Ipp32s)tsse, (Ipp32s)tavx2);
#endif // PRINT_TICKS
}


TEST(optimization, DiffDc_avx2) {
    Ipp32s pitch = 256;
    Ipp32s maxsize = 64;

    auto src8   = utils::MakeAlignedPtr<Ipp8u>(pitch * maxsize, utils::AlignAvx2);
    auto pred8  = utils::MakeAlignedPtr<Ipp8u>(pitch * maxsize, utils::AlignAvx2);
    auto src16  = utils::MakeAlignedPtr<Ipp16u>(pitch * maxsize, utils::AlignAvx2);
    auto pred16 = utils::MakeAlignedPtr<Ipp16u>(pitch * maxsize, utils::AlignAvx2);

    std::minstd_rand0 rand;
    rand.seed(0x1234);

    for (Ipp32s size = 8; size <= 32; size<<=1) {
        utils::InitRandomBlock(rand, src8.get(),  pitch, pitch, maxsize, 0, 255);
        utils::InitRandomBlock(rand, pred8.get(), pitch, pitch, maxsize, 0, 255);
        utils::InitRandomBlock(rand, src16.get(),  pitch, pitch, maxsize, 0, 1023);
        utils::InitRandomBlock(rand, pred16.get(), pitch, pitch, maxsize, 0, 1023);

        {
        std::ostringstream buf;
        buf << "Testing " << size << "x" << size << " 8bit";
        SCOPED_TRACE(buf.str().c_str());
        Ipp32s dc_px   = MFX_HEVC_PP::h265_DiffDc_px  (src8.get(), pitch, pred8.get(), pitch, size);
        Ipp32s dc_avx2 = MFX_HEVC_PP::h265_DiffDc_avx2(src8.get(), pitch, pred8.get(), pitch, size);
        ASSERT_EQ(dc_px, dc_avx2);
        }
        //{
        //std::ostringstream buf;
        //buf << "Testing " << size << "x" << size << " 10bit";
        //SCOPED_TRACE(buf.str().c_str());
        //Ipp32s dc_px   = MFX_HEVC_PP::h265_DiffDc_px  (src16.get(), pitch, pred16.get(), pitch, size);
        //Ipp32s dc_avx2 = MFX_HEVC_PP::h265_DiffDc_px  (src16.get(), pitch, pred16.get(), pitch, size);
        //ASSERT_EQ(dc_px, dc_avx2);
        //}
    }

#ifdef PRINT_TICKS
    for (Ipp32s size = 8; size <= 32; size<<=1) {
        Ipp64u ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffDc_px<Ipp8u>,   src8.get(), pitch, pred8.get(), pitch, size);
        Ipp64u ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffDc_avx2<Ipp8u>, src8.get(), pitch, pred8.get(), pitch, size);
        printf("%2dx%-2d %2dbits speedup = %lld / %lld = %.2f\n", size, size, 8, ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);
    }
    //for (Ipp32s size = 8; size <= 32; size<<=1) {
    //    Ipp64u ticksPx   = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffDc_px<Ipp16u>,   src16.get(), pitch, pred16.get(), pitch, size);
    //    Ipp64u ticksAvx2 = utils::GetMinTicks(10000, MFX_HEVC_PP::h265_DiffDc_avx2<Ipp16u>, src16.get(), pitch, pred16.get(), pitch, size);
    //    printf("%2dx%-2d %2dbits speedup = %lld / %lld = %.2f\n", size, size, 10, ticksPx, ticksAvx2, (double)ticksPx / ticksAvx2);
    //}
#endif //PRINT_TICKS
}