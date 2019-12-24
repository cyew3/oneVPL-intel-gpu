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

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdexcept>
#include <vector>
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"

#pragma comment(lib, "advapi32")
#pragma comment( lib, "ole32.lib" )

#define NEWMVPRED 0

enum {
    SINGLE_REF,
    TWO_REFS,
    COMPOUND
};
#ifdef CMRT_EMU
extern "C"
void MePuGacc64x64(SurfaceIndex SRC, SurfaceIndex REF0, SurfaceIndex REF1, SurfaceIndex MEDATA0, SurfaceIndex MEDATA1, SurfaceIndex OUT_PRED0,
                   uint32_t compoundAllowed, SurfaceIndex SCRATCHPAD);
extern "C"
void MePuGacc32x32(SurfaceIndex SRC, SurfaceIndex REF0, SurfaceIndex REF1, SurfaceIndex MEDATA0, SurfaceIndex MEDATA1, SurfaceIndex OUT_PRED,
                   uint32_t compoundAllowed);
extern "C"
void MePuGacc8x8And16x16(SurfaceIndex SRC, SurfaceIndex REF0, SurfaceIndex REF1,
                         SurfaceIndex MEDATA8_0, SurfaceIndex MEDATA8_1, SurfaceIndex MEDATA16_0, SurfaceIndex MEDATA16_1,
                         SurfaceIndex OUT_PRED8, SurfaceIndex OUT_PRED16, uint32_t compoundAllowed);

const char genx_av1_mepu_hsw[1] = {};
const char genx_av1_mepu_bdw[1] = {};
const char genx_av1_mepu_skl[1] = {};
#else //CMRT_EMU
#include "../include/genx_av1_mepu_hsw_isa.h"
#include "../include/genx_av1_mepu_bdw_isa.h"
#include "../include/genx_av1_mepu_skl_isa.h"
#endif //CMRT_EMU

int Compare(const uint8_t *data1, const uint8_t *data2, int32_t width, int32_t height, int32_t blocksize)
{
    int32_t MaxCUSize = 64;
    int32_t widthInCtbs  = (width + MaxCUSize - 1) / MaxCUSize;
    int32_t heightInCtbs = (height + MaxCUSize - 1) / MaxCUSize;
    int32_t pitch = widthInCtbs * 64;

    for (int y = 0; y < heightInCtbs; y++)
        for (int x = 0; x < widthInCtbs; x++)
            if (data1[y * pitch + x] - data2[y * pitch + x]) {
                printf("BAD pred pixels at (x,y)=(%d,%d) %d vs %d\n", x, y, data1[y * pitch + x], data2[y * pitch + x]);
                return FAILED;
            }

    return PASSED;
}


int CompareBestIdx(const uint8_t *data1, int32_t pitch1, const uint8_t *data2, int32_t pitch2, int32_t width, int32_t height, int32_t blocksize)
{
    int sizeOfElement = blocksize < 2 ? 8 : 64;
    int offset = 0;//sizeOfElement == 8 ? 0 : 12;
    // Luma
    for (int32_t y = 0; y < height >> (3+blocksize); y++) {
        for (int32_t x = 0; x < width >> (3+blocksize); x++) {
            const uint32_t *cmData1 = reinterpret_cast<const uint32_t*>(data1 + y * pitch1 + sizeOfElement * x);
            const uint32_t *cmData2 = reinterpret_cast<const uint32_t*>(data2 + y * pitch2 + sizeOfElement * x);

            uint32_t bestIdx1 = cmData1[1 + offset] & 0x3;
            uint32_t bestIdx2 = cmData2[1 + offset] & 0x3;
            if (bestIdx1 != bestIdx2) {
                printf("BAD BestIdx (blk_x,blk_y)=(%i,%i)\n", x, y);
                return FAILED;
            }
#if 1
            uint32_t satd1 = cmData1[1 + offset] >> 2;
            uint32_t satd2 = cmData2[1 + offset] >> 2;
            if (satd1 != satd2) {
                printf("BAD satd (blk_x,blk_y)=(%i,%i), satd(%i, %i)\n", x, y, satd1, satd2);
                return FAILED;
            }

            if (blocksize >= 2) {
                if( cmData1[0] != cmData1[0]) {
                    printf("BAD MV (blk_x,blk_y)=(%i,%i)\n", x, y);
                    return FAILED;
                }
            }
#endif
        }
    }

    return PASSED;
}

int Dump(uint8_t* data, size_t frameSize, const char* fileName)
{
    FILE *fout = fopen(fileName, "wb");
    if (!fout)
        return FAILED;

    fwrite(data, 1, frameSize, fout);
    fclose(fout);

    return PASSED;
}


class CmRuntimeError : public std::exception
{
public:
    CmRuntimeError() : std::exception() {
        assert(!"CmRuntimeError");
    }
};

enum
{
    MFX_BLK_8x8   = 0,
    MFX_BLK_16x16 = 1,
    MFX_BLK_32x32 = 2,
    MFX_BLK_64x64 = 3,
    MFX_BLK_MAX   = 4,
};

typedef struct
{
    mfxU32 size;
    mfxU32 pitch;
    mfxU32 numBytesInRow;
    mfxU32 numRows;
} mfxSurfInfoENC;


/* leave in same file to avoid difficulties with templates (T is arbitrary type) */
template <class T>
void GetDimensionsCmSurface2DUp(CmDevice *device, uint32_t numElemInRow, uint32_t numRows, CM_SURFACE_FORMAT format, mfxSurfInfoENC *surfInfo)
{
    uint32_t numBytesInRow = numElemInRow * sizeof(T);

    if (surfInfo) {
        memset(surfInfo, 0, sizeof(mfxSurfInfoENC));
        int res = CM_SUCCESS;
        if ((res = device->GetSurface2DInfo(numBytesInRow, numRows, format, surfInfo->pitch, surfInfo->size)) != CM_SUCCESS) {
            throw CmRuntimeError();
        }
        surfInfo->numBytesInRow = numBytesInRow;
        surfInfo->numRows = numRows;
    }
}

template<class T> inline T AlignValue(T value, mfxU32 alignment)
{
    //assert((alignment & (alignment - 1)) == 0); // should be 2^n
    return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
}

static int GetSurfaceDimensions(int32_t width, int32_t height,  mfxSurfInfoENC* interData)
{
    mfxU32 version = 0;
    CmDevice *device = 0;
    int32_t res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    int32_t cmMvW[MFX_BLK_MAX], cmMvH[MFX_BLK_MAX];

    mfxU32 k;
    mfxU32 width32;
    mfxU32 height32;
    mfxU32 width2x;
    mfxU32 height2x;
    mfxU32 width4x;
    mfxU32 height4x;
    mfxU32 interpWidth;
    mfxU32 interpHeight;

    /* pad and align to 16 pixels */
    width32  = AlignValue(width, 32);
    height32 = AlignValue(height, 32);
    width    = AlignValue(width, 16);
    height   = AlignValue(height, 16);

    width2x  = AlignValue(width / 2, 16);
    height2x = AlignValue(height / 2, 16);

    width4x  = AlignValue(width / 4, 16);
    height4x = AlignValue(height / 4, 16);

    /* dimensions of output MV grid for each block size (width/height were aligned to multiple of 16) */
    cmMvW[MFX_BLK_16x16   ] = width / 16;      cmMvH[MFX_BLK_16x16   ] = height / 16;
    cmMvW[MFX_BLK_8x8     ] = width /  8;      cmMvH[MFX_BLK_8x8     ] = height /  8;
    cmMvW[MFX_BLK_32x32   ] = width2x / 16;    cmMvH[MFX_BLK_32x32   ] = height2x / 16;
    cmMvW[MFX_BLK_64x64   ] = width4x / 16;    cmMvH[MFX_BLK_64x64   ] = height4x / 16;

    /* see test_interpolate_frame.cpp */
    interpWidth  = width/*  + 2*MFX_FEI_H265_INTERP_BORDER*/;
    interpHeight = height/* + 2*MFX_FEI_H265_INTERP_BORDER*/;


    /* inter MV and distortion */
    // 1 MV and 9 SADs per block
    k = MFX_BLK_64x64;
    GetDimensionsCmSurface2DUp<mfxU32>(device, cmMvW[k] * 16, cmMvH[k], CM_SURFACE_FORMAT_P8, interData + k);
    k = MFX_BLK_32x32;
    GetDimensionsCmSurface2DUp<mfxU32>(device, cmMvW[k] * 16, cmMvH[k], CM_SURFACE_FORMAT_P8, interData + k);

    // 1 MV + 1 cost
    k = MFX_BLK_16x16;
    GetDimensionsCmSurface2DUp<mfxU32>(device, cmMvW[k] * 2, cmMvH[k], CM_SURFACE_FORMAT_P8, interData + k);
    k = MFX_BLK_8x8;
    GetDimensionsCmSurface2DUp<mfxU32>(device, cmMvW[k] * 2, cmMvH[k], CM_SURFACE_FORMAT_P8, interData + k);

    ::DestroyCmDevice(device);
    return 0;
}

union H265MV {
    struct { int16_t mvx, mvy; };
    uint32_t asInt;
};

struct rand16u {
    rand16u() : val(0) {}
    void init(uint16_t ival) { val = ival; }
    uint16_t val;
    uint16_t operator()() {
        uint32_t bit = ((val >> 0) ^ (val >> 2) ^ (val >> 3) ^ (val >> 5)) & 1;
        val = (val >> 1) | (bit << 15);
        return val;
    }
};

H265MV GenRandMv(rand16u &localrand, int32_t min, int32_t max)
{
    H265MV mv;
    mv.mvx = (localrand() % (max - min + 1)) + min;
    mv.mvy = (localrand() % (max - min + 1)) + min;

    return mv;
}

int32_t GenRandDist(rand16u &localrand, int32_t min, int32_t max)
{
    int32_t dist = (int32_t)((localrand() % (max - min + 1)) + min);

    return dist;
}

void FillRand(rand16u& localrand, uint8_t* data, mfxSurfInfoENC* param, int blockSizeIdx)
{
    int sizeOfElement = (blockSizeIdx >> 1) == 0 ? 8 : 64;
    int elementCount = param->numBytesInRow / sizeOfElement;
    int pitch = param->pitch;

    for (unsigned row = 0; row < param->numRows; row++) {
        for (int element = 0; element < elementCount; element++) {
            H265MV mv   = GenRandMv(localrand, -128, 127);
            uint8_t* _data = data + (row * pitch) + element*sizeOfElement;
            uint32_t* _data32u = reinterpret_cast<uint32_t*>(_data);
            _data32u[0] = mv.asInt;
            _data32u[1] = GenRandDist(localrand, 0, 0xffff >> 4);

            if (blockSizeIdx > 1) {
                for (int i = 0; i < 8; i++) {
                    _data32u[2+i] = GenRandDist(localrand, 0, 0xffff >> 4);
                }
            }
        }
    }
}

#define __ALIGN32 __declspec (align(32))
#define __ALIGN64 __declspec (align(64))

    enum { EIGHTTAP=0, EIGHTTAP_SMOOTH=1, EIGHTTAP_SHARP=2, BILINEAR=3, SWITCHABLE_FILTERS=3,
        SWITCHABLE_FILTER_CONTEXTS=(SWITCHABLE_FILTERS + 1), SWITCHABLE=4 };


void copyROI(uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            dst[y*dstPitch + x] = src[y*srcPitch + x];
        }
    }
}

    static const int32_t SUBPEL_BITS   = 4;
    static const int32_t SUBPEL_SHIFTS = 1 << SUBPEL_BITS;
    static const int32_t SUBPEL_TAPS   = 8;
    typedef int16_t InterpKernel[SUBPEL_TAPS];

    __ALIGN32 static const InterpKernel bilinear_filters[SUBPEL_SHIFTS] = {
        { 0, 0, 0, 128, 0, 0, 0, 0 },  { 0, 0, 0, 120, 8, 0, 0, 0 },
        { 0, 0, 0, 112, 16, 0, 0, 0 }, { 0, 0, 0, 104, 24, 0, 0, 0 },
        { 0, 0, 0, 96, 32, 0, 0, 0 },  { 0, 0, 0, 88, 40, 0, 0, 0 },
        { 0, 0, 0, 80, 48, 0, 0, 0 },  { 0, 0, 0, 72, 56, 0, 0, 0 },
        { 0, 0, 0, 64, 64, 0, 0, 0 },  { 0, 0, 0, 56, 72, 0, 0, 0 },
        { 0, 0, 0, 48, 80, 0, 0, 0 },  { 0, 0, 0, 40, 88, 0, 0, 0 },
        { 0, 0, 0, 32, 96, 0, 0, 0 },  { 0, 0, 0, 24, 104, 0, 0, 0 },
        { 0, 0, 0, 16, 112, 0, 0, 0 }, { 0, 0, 0, 8, 120, 0, 0, 0 }
    };

    // Lagrangian interpolation filter
    __ALIGN32 static const InterpKernel sub_pel_filters_8[SUBPEL_SHIFTS] = {
        { 0, 0, 0, 128, 0, 0, 0, 0 },        { 0, 1, -5, 126, 8, -3, 1, 0 },
        { -1, 3, -10, 122, 18, -6, 2, 0 },   { -1, 4, -13, 118, 27, -9, 3, -1 },
        { -1, 4, -16, 112, 37, -11, 4, -1 }, { -1, 5, -18, 105, 48, -14, 4, -1 },
        { -1, 5, -19, 97, 58, -16, 5, -1 },  { -1, 6, -19, 88, 68, -18, 5, -1 },
        { -1, 6, -19, 78, 78, -19, 6, -1 },  { -1, 5, -18, 68, 88, -19, 6, -1 },
        { -1, 5, -16, 58, 97, -19, 5, -1 },  { -1, 4, -14, 48, 105, -18, 5, -1 },
        { -1, 4, -11, 37, 112, -16, 4, -1 }, { -1, 3, -9, 27, 118, -13, 4, -1 },
        { 0, 2, -6, 18, 122, -10, 3, -1 },   { 0, 1, -3, 8, 126, -5, 1, 0 }
    };

    __ALIGN32 const InterpKernel sub_pel_filters_8_av1[SUBPEL_SHIFTS] = {
        { 0, 0, 0, 128, 0, 0, 0, 0 },      { 0, 2, -6, 126, 8, -2, 0, 0 },
        { 0, 2, -10, 122, 18, -4, 0, 0 },  { 0, 2, -12, 116, 28, -8, 2, 0 },
        { 0, 2, -14, 110, 38, -10, 2, 0 }, { 0, 2, -14, 102, 48, -12, 2, 0 },
        { 0, 2, -16, 94, 58, -12, 2, 0 },  { 0, 2, -14, 84, 66, -12, 2, 0 },
        { 0, 2, -14, 76, 76, -14, 2, 0 },  { 0, 2, -12, 66, 84, -14, 2, 0 },
        { 0, 2, -12, 58, 94, -16, 2, 0 },  { 0, 2, -12, 48, 102, -14, 2, 0 },
        { 0, 2, -10, 38, 110, -14, 2, 0 }, { 0, 2, -8, 28, 116, -12, 2, 0 },
        { 0, 0, -4, 18, 122, -10, 2, 0 },  { 0, 0, -2, 8, 126, -6, 2, 0 }
    };

    // DCT based filter
    __ALIGN32 static const InterpKernel sub_pel_filters_8s[SUBPEL_SHIFTS] = {
        { 0, 0, 0, 128, 0, 0, 0, 0 },         { -1, 3, -7, 127, 8, -3, 1, 0 },
        { -2, 5, -13, 125, 17, -6, 3, -1 },   { -3, 7, -17, 121, 27, -10, 5, -2 },
        { -4, 9, -20, 115, 37, -13, 6, -2 },  { -4, 10, -23, 108, 48, -16, 8, -3 },
        { -4, 10, -24, 100, 59, -19, 9, -3 }, { -4, 11, -24, 90, 70, -21, 10, -4 },
        { -4, 11, -23, 80, 80, -23, 11, -4 }, { -4, 10, -21, 70, 90, -24, 11, -4 },
        { -3, 9, -19, 59, 100, -24, 10, -4 }, { -3, 8, -16, 48, 108, -23, 10, -4 },
        { -2, 6, -13, 37, 115, -20, 9, -4 },  { -2, 5, -10, 27, 121, -17, 7, -3 },
        { -1, 3, -6, 17, 125, -13, 5, -2 },   { 0, 1, -3, 8, 127, -7, 3, -1 }
    };

    __ALIGN32 const InterpKernel sub_pel_filters_8s_av1[SUBPEL_SHIFTS] = {
        { 0, 0, 0, 128, 0, 0, 0, 0 },         { -2, 2, -6, 126, 8, -2, 2, 0 },
        { -2, 6, -12, 124, 16, -6, 4, -2 },   { -2, 8, -18, 120, 26, -10, 6, -2 },
        { -4, 10, -22, 116, 38, -14, 6, -2 }, { -4, 10, -22, 108, 48, -18, 8, -2 },
        { -4, 10, -24, 100, 60, -20, 8, -2 }, { -4, 10, -24, 90, 70, -22, 10, -2 },
        { -4, 12, -24, 80, 80, -24, 12, -4 }, { -2, 10, -22, 70, 90, -24, 10, -4 },
        { -2, 8, -20, 60, 100, -24, 10, -4 }, { -2, 8, -18, 48, 108, -22, 10, -4 },
        { -2, 6, -14, 38, 116, -22, 10, -4 }, { -2, 6, -10, 26, 120, -18, 8, -2 },
        { -2, 4, -6, 16, 124, -12, 6, -2 },   { 0, 2, -2, 8, 126, -6, 2, -2 }
    };

    // freqmultiplier = 0.5
    __ALIGN32 static const InterpKernel sub_pel_filters_8lp[SUBPEL_SHIFTS] = {
        { 0, 0, 0, 128, 0, 0, 0, 0 },       { -3, -1, 32, 64, 38, 1, -3, 0 },
        { -2, -2, 29, 63, 41, 2, -3, 0 },   { -2, -2, 26, 63, 43, 4, -4, 0 },
        { -2, -3, 24, 62, 46, 5, -4, 0 },   { -2, -3, 21, 60, 49, 7, -4, 0 },
        { -1, -4, 18, 59, 51, 9, -4, 0 },   { -1, -4, 16, 57, 53, 12, -4, -1 },
        { -1, -4, 14, 55, 55, 14, -4, -1 }, { -1, -4, 12, 53, 57, 16, -4, -1 },
        { 0, -4, 9, 51, 59, 18, -4, -1 },   { 0, -4, 7, 49, 60, 21, -3, -2 },
        { 0, -4, 5, 46, 62, 24, -3, -2 },   { 0, -4, 4, 43, 63, 26, -2, -2 },
        { 0, -3, 2, 41, 63, 29, -2, -2 },   { 0, -3, 1, 38, 64, 32, -1, -3 }
    };

    __ALIGN32 const InterpKernel sub_pel_filters_8lp_av1[SUBPEL_SHIFTS] = {
        { 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 2, 28, 62, 34, 2, 0, 0 },
        { 0, 0, 26, 62, 36, 4, 0, 0 },    { 0, 0, 22, 62, 40, 4, 0, 0 },
        { 0, 0, 20, 60, 42, 6, 0, 0 },    { 0, 0, 18, 58, 44, 8, 0, 0 },
        { 0, 0, 16, 56, 46, 10, 0, 0 },   { 0, -2, 16, 54, 48, 12, 0, 0 },
        { 0, -2, 14, 52, 52, 14, -2, 0 }, { 0, 0, 12, 48, 54, 16, -2, 0 },
        { 0, 0, 10, 46, 56, 16, 0, 0 },   { 0, 0, 8, 44, 58, 18, 0, 0 },
        { 0, 0, 6, 42, 60, 20, 0, 0 },    { 0, 0, 4, 40, 62, 22, 0, 0 },
        { 0, 0, 4, 36, 62, 26, 0, 0 },    { 0, 0, 2, 34, 62, 28, 2, 0 }
    };

    const InterpKernel *vp9_filter_kernels[4] = {
        sub_pel_filters_8, sub_pel_filters_8lp, sub_pel_filters_8s, bilinear_filters
    };

    const InterpKernel *av1_filter_kernels[4] = {
        sub_pel_filters_8_av1, sub_pel_filters_8lp_av1, sub_pel_filters_8s_av1, bilinear_filters
    };

typedef void (*convolve_fn_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *filtx, const int16_t *filty, int w, int h);



typedef int16_t int16_t;
typedef uint8_t  uint8_t;
#define FILTER_BITS 7
#define SUBPEL_BITS 4
#define SUBPEL_MASK ((1 << SUBPEL_BITS) - 1)
#define SUBPEL_SHIFTS (1 << SUBPEL_BITS)
#define SUBPEL_TAPS 8
static const InterpKernel *get_filter_base(const int16_t *filter)
{
    // NOTE: This assumes that the filter table is 256-byte aligned.
    // TODO(agrange) Modify to make independent of table alignment.
    return (const InterpKernel *)(((intptr_t)filter) & ~((intptr_t)0xFF));
}

static int get_filter_offset(const int16_t *f, const InterpKernel *base)
{
    return (int)((const InterpKernel *)(intptr_t)f - base);
}

    /* Shift down with rounding */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n)-1))) >> (n))

static inline uint8_t clip_pixel(int val)
{
    return (val > 255) ? 255 : (val < 0) ? 0 : val;
}

template <bool avg> void convolve8_copy_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
{
    int r;

    (void)filter_x;
    (void)filter_y;

    for (r = h; r > 0; --r) {
        if (avg)
            for (int x = 0; x < w; ++x) dst[x] = ROUND_POWER_OF_TWO(dst[x] + src[x], 1);
        else
            memcpy(dst, src, w);
        src += src_stride;
        dst += dst_stride;
    }
}

static void core_convolve_avg_vert(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride,
                                   const InterpKernel *y_filters, int y0_q4,
                                   int y_step_q4, int w, int h)
{
    int x, y;
    src -= src_stride * (SUBPEL_TAPS / 2 - 1);

    for (x = 0; x < w; ++x) {
        int y_q4 = y0_q4;
        for (y = 0; y < h; ++y) {
            const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
            const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k)
                sum += src_y[k * src_stride] * y_filter[k];
            dst[y * dst_stride] = ROUND_POWER_OF_TWO(
                dst[y * dst_stride] +
                clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)),
                1);
            y_q4 += y_step_q4;
        }
        ++src;
        ++dst;
    }
}

static void core_convolve_vert(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
    const InterpKernel *y_filters, int y0_q4, int y_step_q4, int w, int h)
{
    int x, y;
    src -= src_stride * (SUBPEL_TAPS / 2 - 1);

    for (x = 0; x < w; ++x) {
        int y_q4 = y0_q4;
        for (y = 0; y < h; ++y) {
            const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
            const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k)
                sum += src_y[k * src_stride] * y_filter[k];
            dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
            y_q4 += y_step_q4;
        }
        ++src;
        ++dst;
    }
}

template <bool avg> void convolve8_vert_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
{
    const InterpKernel *const filters_y = get_filter_base(filter_y);
    const int y0_q4 = get_filter_offset(filter_y, filters_y);

    (void)filter_x;

    (avg)
        ? core_convolve_avg_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4, 16, w, h)
        : core_convolve_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4, 16, w, h);
}

static void core_convolve_avg_horz(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride,
                                   const InterpKernel *x_filters, int x0_q4,
                                   int x_step_q4, int w, int h)
{
    int x, y;
    src -= SUBPEL_TAPS / 2 - 1;
    for (y = 0; y < h; ++y) {
        int x_q4 = x0_q4;
        for (x = 0; x < w; ++x) {
            const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
            const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
            dst[x] = ROUND_POWER_OF_TWO(
                dst[x] + clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);
            x_q4 += x_step_q4;
        }
        src += src_stride;
        dst += dst_stride;
    }
}

static void core_convolve_horz(const uint8_t *src, int src_stride,
                               uint8_t *dst, int dst_stride,
                               const InterpKernel *x_filters, int x0_q4,
                               int x_step_q4, int w, int h)
{
    int x, y;
    src -= SUBPEL_TAPS / 2 - 1;
    for (y = 0; y < h; ++y) {
        int x_q4 = x0_q4;
        for (x = 0; x < w; ++x) {
            const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
            const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
            dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
            x_q4 += x_step_q4;
        }
        src += src_stride;
        dst += dst_stride;
    }
}

template <bool avg> void convolve8_horz_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
{
    const InterpKernel *const filters_x = get_filter_base(filter_x);
    const int x0_q4 = get_filter_offset(filter_x, filters_x);

    (void)filter_y;

    (avg)
        ? core_convolve_avg_horz(src, src_stride, dst, dst_stride, filters_x, x0_q4, 16, w, h)
        : core_convolve_horz(src, src_stride, dst, dst_stride, filters_x, x0_q4, 16, w, h);
}

template <bool avg> void convolve8_both_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
{
    const InterpKernel *const filters_x = get_filter_base(filter_x);
    const int x0_q4 = get_filter_offset(filter_x, filters_x);

    const InterpKernel *const filters_y = get_filter_base(filter_y);
    const int y0_q4 = get_filter_offset(filter_y, filters_y);

    // Note: Fixed size intermediate buffer, temp, places limits on parameters.
    // 2d filtering proceeds in 2 steps:
    //   (1) Interpolate horizontally into an intermediate buffer, temp.
    //   (2) Interpolate temp vertically to derive the sub-pixel result.
    // Deriving the maximum number of rows in the temp buffer (135):
    // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
    // --Largest block size is 64x64 pixels.
    // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
    //   original frame (in 1/16th pixel units).
    // --Must round-up because block may be located at sub-pixel position.
    // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
    // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
    uint8_t temp[135 * 64];
    int intermediate_height = (((h - 1) * 16 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

    assert(w <= 64);
    assert(h <= 64);

    if (avg) {
        convolve8_both_px<false>(src, src_stride, temp, 64, filter_x, filter_y, w, h);
        convolve8_copy_px<true>(temp, 64, dst, dst_stride, NULL, NULL, w, h);
    } else {
        core_convolve_horz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64, filters_x, x0_q4, 16, w, intermediate_height);
        core_convolve_vert(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filters_y, y0_q4, 16, w, h);
    }
}

void interpolate_luma_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, int subpel_x, int subpel_y, int w, int h, int ref, int32_t interp_filter, int32_t isAV1)
{
    convolve_fn_t predict_px[2][2][2]; // horiz, vert, avg
    predict_px[0][0][0] = convolve8_copy_px<false>;
    predict_px[0][1][0] = convolve8_vert_px<false>;
    predict_px[1][0][0] = convolve8_horz_px<false>;
    predict_px[1][1][0] = convolve8_both_px<false>;
    predict_px[0][0][1] = convolve8_copy_px<true>;
    predict_px[0][1][1] = convolve8_vert_px<true>;
    predict_px[1][0][1] = convolve8_horz_px<true>;
    predict_px[1][1][1] = convolve8_both_px<true>;

    const InterpKernel *kernel = isAV1 ? av1_filter_kernels[interp_filter] : vp9_filter_kernels[interp_filter];
    predict_px[subpel_x != 0][subpel_y != 0][ref](src, pitchSrc, dst, pitchDst, kernel[subpel_x], kernel[subpel_y], w, h);
}

void Interpolate(const uint8_t *refColoc, int32_t refPitch, uint8_t *dst, int32_t dstPitch, H265MV mv, int32_t w, int32_t h, int32_t avg, int32_t interp, int32_t isAV1)
{
    const int32_t intx = mv.mvx >> 3;
    const int32_t inty = mv.mvy >> 3;
    const int32_t dx = (mv.mvx << 1) & 15;
    const int32_t dy = (mv.mvy << 1) & 15;
    refColoc += intx + inty * refPitch;
    interpolate_luma_px(refColoc, refPitch, dst, dstPitch, dx, dy, w, h, avg, interp, isAV1);
}

int sad_general_px(const uint8_t *p1, int pitch1, const uint8_t *p2, int pitch2, int W, int H)
{
    int sum = 0;
    for (int i = 0; i < H; i++, p1+=pitch1, p2+=pitch2)
        for (int j = 0; j < W; j++)
            sum += ABS(p1[j] - p2[j]);
    return sum;
}


void average_px(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int w, int h)
{
    for (int y = 0; y < h; y++, src1 += pitchSrc1, src2 += pitchSrc2, dst += pitchDst)
        for (int x = 0; x < w; x++)
            dst[x] = (uint8_t)((unsigned(src1[x]) + unsigned(src2[x]) + 1) >> 1);
}

struct Limits {
    int16_t HorMin, HorMax, VerMin, VerMax;
};

static const int16_t DXDY1[9][2] = {
    {-1, -1}, {0, -1}, {1, -1},
    {-1,  0}, {0,  0}, {1,  0},
    {-1,  1}, {0,  1}, {1,  1}
};
static const int16_t DXDY2[9][2] = {
    {-2, -2}, {0, -2}, {2, -2},
    {-2,  0}, {0,  0}, {2,  0},
    {-2,  2}, {0,  2}, {2,  2}
};

#define IPP_ABS( a ) ( ((a) < 0) ? (-(a)) : (a) )
typedef uint8_t PixType;
int32_t SATD8x8(const uint8_t* pSrcCur, int srcCurStep, const uint8_t* pSrcRef, int srcRefStep)
{
    int32_t i, j;
    uint32_t satd = 0;
    int16_t diff[8][8];

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            diff[i][j] = (int16_t)(pSrcCur[j] - pSrcRef[j]);
        pSrcCur += srcCurStep;
        pSrcRef += srcRefStep;
    }

    for (i = 0; i < 8; i++) {
        int32_t t0 = diff[i][0] + diff[i][4];
        int32_t t4 = diff[i][0] - diff[i][4];
        int32_t t1 = diff[i][1] + diff[i][5];
        int32_t t5 = diff[i][1] - diff[i][5];
        int32_t t2 = diff[i][2] + diff[i][6];
        int32_t t6 = diff[i][2] - diff[i][6];
        int32_t t3 = diff[i][3] + diff[i][7];
        int32_t t7 = diff[i][3] - diff[i][7];
        int32_t s0 = t0 + t2;
        int32_t s2 = t0 - t2;
        int32_t s1 = t1 + t3;
        int32_t s3 = t1 - t3;
        int32_t s4 = t4 + t6;
        int32_t s6 = t4 - t6;
        int32_t s5 = t5 + t7;
        int32_t s7 = t5 - t7;
        diff[i][0] = s0 + s1;
        diff[i][1] = s0 - s1;
        diff[i][2] = s2 + s3;
        diff[i][3] = s2 - s3;
        diff[i][4] = s4 + s5;
        diff[i][5] = s4 - s5;
        diff[i][6] = s6 + s7;
        diff[i][7] = s6 - s7;
    }
    for (i = 0; i < 8; i++) {
        int32_t t0 = diff[0][i] + diff[4][i];
        int32_t t4 = diff[0][i] - diff[4][i];
        int32_t t1 = diff[1][i] + diff[5][i];
        int32_t t5 = diff[1][i] - diff[5][i];
        int32_t t2 = diff[2][i] + diff[6][i];
        int32_t t6 = diff[2][i] - diff[6][i];
        int32_t t3 = diff[3][i] + diff[7][i];
        int32_t t7 = diff[3][i] - diff[7][i];
        int32_t s0 = t0 + t2;
        int32_t s2 = t0 - t2;
        int32_t s1 = t1 + t3;
        int32_t s3 = t1 - t3;
        int32_t s4 = t4 + t6;
        int32_t s6 = t4 - t6;
        int32_t s5 = t5 + t7;
        int32_t s7 = t5 - t7;
        satd += IPP_ABS(s0 + s1);
        satd += IPP_ABS(s0 - s1);
        satd += IPP_ABS(s2 + s3);
        satd += IPP_ABS(s2 - s3);
        satd += IPP_ABS(s4 + s5);
        satd += IPP_ABS(s4 - s5);
        satd += IPP_ABS(s6 + s7);
        satd += IPP_ABS(s6 - s7);
    }

    return satd;
}

void SATD8x8Pair(const uint8_t* pSrcCur, int srcCurStep, const uint8_t* pSrcRef, int srcRefStep, int32_t* satdPair)
{
    satdPair[0] = SATD8x8(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = SATD8x8(pSrcCur + 8, srcCurStep, pSrcRef + 8, srcRefStep);
}

int32_t satdNxN(const PixType *src, int32_t pitchSrc, const PixType *rec, int32_t pitchRec, int32_t size)
{
    uint32_t satdTotal = 0;
    int32_t satd[2] = {0, 0};

    /* assume height and width are multiple of 4 */
    assert(size == 4 || size == 8 || size == 16 || size == 32 || size == 64);

    switch (size) {
        case 64:
            for (int32_t j = 0; j < 64; j += 8, src += pitchSrc * 8, rec += pitchRec * 8) {
                SATD8x8Pair(src + 0,  pitchSrc, rec + 0,  pitchRec, satd);
                //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
                satdTotal += satd[0] + satd[1];
                SATD8x8Pair(src + 16, pitchSrc, rec + 16, pitchRec, satd);
                //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
                satdTotal += satd[0] + satd[1];
                SATD8x8Pair(src + 32, pitchSrc, rec + 32, pitchRec, satd);
                //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
                satdTotal += satd[0] + satd[1];
                SATD8x8Pair(src + 48, pitchSrc, rec + 48, pitchRec, satd);
                //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
                satdTotal += satd[0] + satd[1];
            }
            satdTotal = (satdTotal + 2) >> 2;
            break;
    case 32:
        for (int32_t j = 0; j < 32; j += 8, src += pitchSrc * 8, rec += pitchRec * 8) {
            SATD8x8Pair(src + 0,  pitchSrc, rec + 0,  pitchRec, satd);
            //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
            satdTotal += satd[0] + satd[1];
            SATD8x8Pair(src + 16, pitchSrc, rec + 16, pitchRec, satd);
            //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
            satdTotal += satd[0] + satd[1];
        }
        satdTotal = (satdTotal + 2) >> 2;
        break;
    case 16:
        SATD8x8Pair(src, pitchSrc, rec, pitchRec, satd);
        satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
        src += pitchSrc * 8;
        rec += pitchRec * 8;
        SATD8x8Pair(src, pitchSrc, rec, pitchRec, satd);
        satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
        break;
    case 8:
        satd[0] = SATD8x8(src, pitchSrc, rec, pitchRec);
        satdTotal += (satd[0] + 2) >> 2;
        break;
    default: assert(0);
    }
    return satdTotal;
}

void MePuGacc_px(
    int32_t blocksize, //idx
    uint8_t* src, int32_t srcPitch,
    uint8_t* medata0,
    uint8_t* medata1,
    uint8_t* ref0, int32_t ref0Pitch,
    uint8_t* ref1, int32_t ref1Pitch,
    int refConfig,
    uint8_t* outPred, int32_t predPitch,
    Limits & limits,
    int debug,
    int isAV1)
{
    const int32_t w = 1 << (3 + blocksize);
    const int32_t h = w;
    const int Log2MaxCUSize = 6;// blocks 64x64
    int32_t depth = 3 - blocksize;

    uint8_t* medata[2] = {medata0, medata1};
    uint8_t* ref[2] = {ref0, ref1};
    int32_t refPitch[2] = {ref0Pitch, ref1Pitch};
    uint8_t interpBufs[3*64*64];
    uint8_t *predictions[3] = {interpBufs,interpBufs+64*64, interpBufs+2*64*64};

    int32_t bestSad = INT_MAX;
    int32_t bestRefIdx = 0;

    uint32_t costs[3] = {0};
    H265MV bestMv_64B[2];
    const int32_t numRefs = (refConfig == SINGLE_REF) ? 1 : 2;
    for (int32_t refIdx = 0; refIdx < numRefs; refIdx++) {
        H265MV bestMv;
        if (w == 64) {
            const uint32_t *cmData = reinterpret_cast<const uint32_t*>(medata[refIdx]);
            const H265MV *mv = reinterpret_cast<const H265MV*>(cmData);
            const uint32_t *dists = cmData + 1;
            uint64_t bestCost = ULLONG_MAX;
            for (int32_t i = 0; i < 9; i++) {
                H265MV mvcand = { int16_t(mv->mvx + DXDY2[i][0]), int16_t(mv->mvy + DXDY2[i][1]) };
                uint64_t cost = dists[i];
                if (bestCost > cost) {
                    bestCost = cost;
                    bestMv.asInt = mvcand.asInt;
                }
            }
            bestMv_64B[refIdx] = bestMv;
        } else if (w == 32) {
            const uint32_t *cmData = reinterpret_cast<const uint32_t*>(medata[refIdx]);
            const H265MV *mv = reinterpret_cast<const H265MV*>(cmData);
            const uint32_t *dists = cmData + 1;
            uint64_t bestCost = ULLONG_MAX;
            for (int32_t i = 0; i < 9; i++) {
                H265MV mvcand = { int16_t(mv->mvx + DXDY1[i][0]), int16_t(mv->mvy + DXDY1[i][1]) };
                uint64_t cost = dists[i];
                if (bestCost > cost) {
                    bestCost = cost;
                    bestMv.asInt = mvcand.asInt;
                }
            }
            bestMv_64B[refIdx] = bestMv;
        } else if (w <= 16) {
            const uint32_t *cmData = reinterpret_cast<const uint32_t*>(medata[refIdx]);
            const H265MV *mv = reinterpret_cast<const H265MV*>(cmData);
            bestMv.asInt = mv->asInt;
        }

        bestMv.mvx <<= 1;
        bestMv.mvy <<= 1;
        bestMv.mvx = CLIPVAL(bestMv.mvx, limits.HorMin, limits.HorMax);
        bestMv.mvy = CLIPVAL(bestMv.mvy, limits.VerMin, limits.VerMax);

        Interpolate(ref[refIdx], refPitch[refIdx], predictions[refIdx], 64, bestMv, w, h, 0, EIGHTTAP, isAV1);
        int32_t sad = sad_general_px(src, srcPitch, predictions[refIdx], 64, w, h);
        if (bestSad > sad) {
            bestRefIdx = refIdx;
            bestSad = sad;
        }
        costs[refIdx] = sad;
    }

    if (refConfig == COMPOUND) {
        average_px(predictions[0], 64, predictions[1], 64, predictions[2], 64, w, h);
        int32_t sad = sad_general_px(src, srcPitch, predictions[2], 64, w, h);
        if (bestSad > sad) {
            bestRefIdx = 2;
            bestSad = sad;
        }
        costs[2] = sad;
    }

    if (debug) {
        printf("\n %i %i %i \n", costs[0], costs[1], costs[2]);
    }

    // debug
    //bestRefIdx = 2;

    // best candidate report (refIdx & predictor)
    copyROI(predictions[bestRefIdx], 64, outPred, predPitch, w, h);
    int32_t satd = isAV1
        ? sad_general_px(src, srcPitch, outPred, predPitch, w, h)
        : satdNxN(src, srcPitch, outPred, predPitch, w);
    //int32_t satd = sad_general_px(src, srcPitch, outPred, predPitch, w,h);
    uint32_t *cmData0 = (uint32_t*)(medata0);
    uint32_t *cmData1 = (uint32_t*)(medata1);

    int sizeOfElement = blocksize < 2 ? 8 : 64;
    int offset = 0;//sizeOfElement == 8 ? 0 : 12;
    cmData0[1 + offset] = (satd << 2) + bestRefIdx;
    if (sizeOfElement == 64) {
        cmData1[1 + offset] = (satd << 2) + bestRefIdx;

        cmData0[0 + offset] = bestMv_64B[0].asInt;
        cmData1[0 + offset] = bestMv_64B[1].asInt;
    }
}

#define IPP_MIN_16S    (-32768 )
#define IPP_MAX_16S    ( 32767 )

int RunCpu(int width, int height, int MaxCUSize,
            int blocksize,
            uint8_t* src, int32_t srcPitch,
            uint8_t* ref0, int32_t ref0Pitch,
            uint8_t* medata0, int32_t medata0Pitch,
            uint8_t*ref1, int32_t ref1Pitch,
            uint8_t* medata1, int32_t medata1Pitch,
            uint8_t* outPred, int32_t outPredPitch,
            int refConfig,
            int isAV1)
{
    int blockW = 1 << (3 + blocksize);
    int blockH = 1 << (3 + blocksize);
    int32_t predPitch = 64;
    int32_t widthInCtbs = (width + MaxCUSize - 1) / MaxCUSize;

    for (int32_t y = 0; y < height; y += blockH) {
        for (int32_t x = 0; x < width; x += blockW) {
            int32_t ctbPelX = (x / MaxCUSize) * MaxCUSize;
            int32_t ctbPelY = (y / MaxCUSize) * MaxCUSize;
            int32_t ctbx = ctbPelX / MaxCUSize;
            int32_t ctby = ctbPelY / MaxCUSize;
            int32_t ctb = ctby * widthInCtbs + ctbx;
            int32_t locx = x - ctbPelX;
            int32_t locy = y - ctbPelY;

            // simulate limits to clip MV
            const int32_t MvShift = 3;
            const int32_t offset = 8;
            Limits lim;
            lim.HorMax = MIN((width + offset - ctbPelX - 1) << MvShift, IPP_MAX_16S);
            lim.HorMin = MAX((-MaxCUSize - offset - ctbPelX + 1) << MvShift, IPP_MIN_16S);
            lim.VerMax = MIN((height + offset - ctbPelY - 1) << MvShift, IPP_MAX_16S);
            lim.VerMin = MAX((-MaxCUSize - offset - ctbPelY + 1) << MvShift, IPP_MIN_16S);

            int32_t depth = 3 - blocksize;
            const int32_t col = x >> (6 - depth);
            const int32_t row = y >> (6 - depth);
            int32_t sizeOfElement = (blocksize > 1)  ? 64 : 8;

            int debug = 0;

            MePuGacc_px(blocksize,
                src + x + y*srcPitch, srcPitch,
                medata0 + row * medata0Pitch + sizeOfElement*col,
                medata1 + row * medata0Pitch + sizeOfElement*col,
                ref0 + x + y*ref0Pitch, ref0Pitch,
                ref1 + x + y*ref1Pitch, ref1Pitch,
                refConfig,
                outPred + x + y*outPredPitch,
                outPredPitch,
                lim,
                debug,
                isAV1);
        }
    }

    return 0;
}

int RunGpu(int width, int height, int MaxCUSize,
            int blocksize,
            uint8_t* src, int32_t srcPitch,
            uint8_t* ref0, int32_t ref0Pitch,
            uint8_t* data0, int32_t data0Pitch,
            uint8_t*ref1, int32_t ref1Pitch,
            uint8_t* data1, int32_t data1Pitch,
            uint8_t* outPred,
            int refConfig,
            mfxSurfInfoENC &medataParam,
            uint32_t isAV1)
{
    mfxU32 version = 0;
    CmDevice *device = 0;
    int32_t res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    GPU_PLATFORM hw_type;
    size_t size = sizeof(mfxU32);
    res = device->GetCaps(CAP_GPU_PLATFORM, size, &hw_type);
    CHECK_CM_ERR(res);
#ifdef CMRT_EMU
    hw_type = PLATFORM_INTEL_HSW;
#endif // CMRT_EMU
    switch (hw_type) {
    case PLATFORM_INTEL_HSW:
        res = device->LoadProgram((void *)genx_av1_mepu_hsw, sizeof(genx_av1_mepu_hsw), program, "nojitter");
        break;
    case PLATFORM_INTEL_BDW:
    case PLATFORM_INTEL_CHV:
        res = device->LoadProgram((void *)genx_av1_mepu_bdw, sizeof(genx_av1_mepu_bdw), program, "nojitter");
        break;
    case PLATFORM_INTEL_SKL:
    case PLATFORM_INTEL_KBL:
        res = device->LoadProgram((void *)genx_av1_mepu_skl, sizeof(genx_av1_mepu_skl), program, "nojitter");
        break;
    default:
        return FAILED;
    }
    CHECK_CM_ERR(res);

    int32_t isTGMode = 0;
    CmKernel *kernel = 0;
    if (blocksize == MFX_BLK_8x8) {
        assert(0);
    } else if (blocksize == MFX_BLK_16x16) {
        assert(0);
    } else if (blocksize == MFX_BLK_32x32) {
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(MePuGacc32x32), kernel);
        isTGMode = 0;
    } else if (blocksize == MFX_BLK_64x64) {
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(MePuGacc64x64), kernel);
        isTGMode = 0;
    }
    CHECK_CM_ERR(res);

    // debug printf info
    //res = device->InitPrintBuffer();
    //CHECK_CM_ERR(res);

    // SRC_LU, REF0_LU, REF1_LU, OUTPRED_LU
    CmSurface2D *inSrc = 0;
    res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, inSrc);
    CHECK_CM_ERR(res);
    res = inSrc->WriteSurfaceStride(src, NULL, srcPitch);
    CHECK_CM_ERR(res);

    CmSurface2D *inRef0 = 0;
    res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, inRef0);
    CHECK_CM_ERR(res);
    res = inRef0->WriteSurfaceStride(ref0, NULL, ref0Pitch);
    CHECK_CM_ERR(res);

    CmSurface2D *inRef1 = 0;
    res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, inRef1);
    CHECK_CM_ERR(res);
    res = inRef1->WriteSurfaceStride(ref1, NULL, ref1Pitch);
    CHECK_CM_ERR(res);

    // medata from memory
    CmSurface2D* medata0;
    res = device->CreateSurface2D(medataParam.numBytesInRow, medataParam.numRows, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), medata0);
    CHECK_CM_ERR(res);
    res = medata0->WriteSurfaceStride(data0, NULL, data0Pitch);
    CHECK_CM_ERR(res);

    CmSurface2D* medata1;
    res = device->CreateSurface2D(medataParam.numBytesInRow, medataParam.numRows, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), medata1);
    CHECK_CM_ERR(res);
    res = medata1->WriteSurfaceStride(data1, NULL, data1Pitch);
    CHECK_CM_ERR(res);

    // out predictors
    //int32_t sizeofPred = (width+63)/64 * (height+63)/64 * (64*64);
    int32_t widthInCtbs  = (width + MaxCUSize - 1) / MaxCUSize;
    int32_t heightInCtbs = (height+ MaxCUSize - 1) / MaxCUSize;
    int32_t numCtbs = widthInCtbs * heightInCtbs;
    int32_t sizeofPred = numCtbs * 64 * 64;

    CmSurface2D *outPredCm = 0;
    res = device->CreateSurface2D(widthInCtbs*64, heightInCtbs*64, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), outPredCm);
    CHECK_CM_ERR(res);

    CmSurface2D *outPredCm2 = 0;
    res = device->CreateSurface2D(widthInCtbs*64, heightInCtbs*64*2, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), outPredCm2);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInSrc = 0;
    res = inSrc->GetIndex(idxInSrc);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInRef0 = 0;
    res = inRef0->GetIndex(idxInRef0);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInRef1 = 0;
    res = inRef1->GetIndex(idxInRef1);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxMeData0 = 0;
    res = medata0->GetIndex(idxMeData0);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxMeData1 = 0;
    res = medata1->GetIndex(idxMeData1);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutPred = 0;
    res = outPredCm->GetIndex(idxOutPred);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutPred2 = 0;
    res = outPredCm2->GetIndex(idxOutPred2);
    CHECK_CM_ERR(res);


    const mfxU16 BlockW = 1 << (3 + blocksize);
    const mfxU16 BlockH = BlockW;
    mfxU32 tsWidth = (width + BlockW - 1) / BlockW;
    mfxU32 tsHeight = (height + BlockH - 1) / BlockH;

    CmThreadSpace * threadSpace = 0;
    CmThreadGroupSpace* pTGS = NULL;
    if (isTGMode == 0) {
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);
        res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
        CHECK_CM_ERR(res);
        res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
        CHECK_CM_ERR(res);
    } else {
        int groupWidth   = (width + BlockW - 1) / BlockW;
        int groupHeight  = (height + BlockH - 1) / BlockH;
        tsWidth = 4;
        tsHeight= 4;
        int threadCount = (groupWidth * groupHeight) * (tsWidth * tsHeight);
        res = kernel->SetThreadCount(threadCount);
        CHECK_CM_ERR(res);
        res = device->CreateThreadGroupSpace(tsWidth, tsHeight, groupWidth, groupHeight, pTGS);
        CHECK_CM_ERR(res);

        device->SetSuggestedL3Config(SKL_L3_PLANE_5);
    }

    UINT argIdx = 0;
    res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxInSrc);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxInRef0);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxInRef1);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxMeData0);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxMeData1);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxOutPred);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(argIdx++, sizeof(uint32_t), &refConfig);
    CHECK_CM_ERR(res);
    if (blocksize == MFX_BLK_64x64) {
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxOutPred2);
        CHECK_CM_ERR(res);
    }

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);
    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    CmEvent * e = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    //res = queue->Enqueue(task, e, threadSpace);
    res = isTGMode ? queue->EnqueueWithGroup(task, e, pTGS) : queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    // copy out data
    res = medata0->ReadSurfaceStride(data0, NULL, data0Pitch);
    CHECK_CM_ERR(res);
    res = medata1->ReadSurfaceStride(data1, NULL, data1Pitch);
    CHECK_CM_ERR(res);
    res = outPredCm->ReadSurface(outPred, NULL);

    //device->FlushPrintBuffer();

#if !defined CMRT_EMU
    if (isTGMode)
        printf("TIME = %.3f ms ",  GetAccurateGpuTime_ThreadGroup(queue, task, pTGS) / 1000000.0);
    else
        printf("TIME = %.3f ms ",  GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif

    device->DestroyTask(task);

    if (blocksize == MFX_BLK_64x64) {
        res = device->DestroySurface(outPredCm2);
    }
    res = device->DestroySurface(outPredCm);
    device->DestroySurface(medata0);
    device->DestroySurface(medata1);

    device->DestroySurface(inRef1);
    device->DestroySurface(inRef0);
    device->DestroySurface(inSrc);

    if (isTGMode)
        device->DestroyThreadGroupSpace(pTGS);
    else
        device->DestroyThreadSpace(threadSpace);

    device->DestroyKernel(kernel);
    device->DestroyProgram(program);

    ::DestroyCmDevice(device);

    return PASSED;
}


int RunGpuCombine8x16(int width, int height, int MaxCUSize,
            uint8_t* src, int32_t srcPitch,
            uint8_t* ref0, int32_t ref0Pitch,
            uint8_t* data8_0, int32_t data8_0Pitch,
            uint8_t* data16_0, int32_t data16_0Pitch,
            uint8_t*ref1, int32_t ref1Pitch,
            uint8_t* data8_1, int32_t data8_1Pitch,
            uint8_t* data16_1, int32_t data16_1Pitch,
            uint8_t* outPred8,
            uint8_t* outPred16,
            int refConfig,
            mfxSurfInfoENC &medataParam8,
            mfxSurfInfoENC &medataParam16,
            uint32_t isAV1)
{
    mfxU32 version = 0;
    CmDevice *device = 0;
    int32_t res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    GPU_PLATFORM hw_type;
    size_t size = sizeof(mfxU32);
    res = device->GetCaps(CAP_GPU_PLATFORM, size, &hw_type);
    CHECK_CM_ERR(res);
#ifdef CMRT_EMU
    hw_type = PLATFORM_INTEL_HSW;
#endif // CMRT_EMU
    switch (hw_type) {
    case PLATFORM_INTEL_HSW:
        res = device->LoadProgram((void *)genx_av1_mepu_hsw, sizeof(genx_av1_mepu_hsw), program, "nojitter");
        break;
    case PLATFORM_INTEL_BDW:
    case PLATFORM_INTEL_CHV:
        res = device->LoadProgram((void *)genx_av1_mepu_bdw, sizeof(genx_av1_mepu_bdw), program, "nojitter");
        break;
    case PLATFORM_INTEL_SKL:
    case PLATFORM_INTEL_KBL:
        res = device->LoadProgram((void *)genx_av1_mepu_skl, sizeof(genx_av1_mepu_skl), program, "nojitter");
        break;
    default:
        return FAILED;
    }
    CHECK_CM_ERR(res);

    int32_t isTGMode = 0;
    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(MePuGacc8x8And16x16), kernel);
    CHECK_CM_ERR(res);

    // debug printf info
    //res = device->InitPrintBuffer();
    //CHECK_CM_ERR(res);

    // SRC_LU, REF0_LU, REF1_LU, OUTPRED_LU
    CmSurface2D *inSrc = 0;
    res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, inSrc);
    CHECK_CM_ERR(res);
    res = inSrc->WriteSurfaceStride(src, NULL, srcPitch);
    CHECK_CM_ERR(res);

    CmSurface2D *inRef0 = 0;
    res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, inRef0);
    CHECK_CM_ERR(res);
    res = inRef0->WriteSurfaceStride(ref0, NULL, ref0Pitch);
    CHECK_CM_ERR(res);

    CmSurface2D *inRef1 = 0;
    res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, inRef1);
    CHECK_CM_ERR(res);
    res = inRef1->WriteSurfaceStride(ref1, NULL, ref1Pitch);
    CHECK_CM_ERR(res);

    // medata from memory
    // 8x8
    CmSurface2D* medata8_0;
    res = device->CreateSurface2D(medataParam8.numBytesInRow, medataParam8.numRows, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), medata8_0);
    CHECK_CM_ERR(res);
    res = medata8_0->WriteSurfaceStride(data8_0, NULL, data8_0Pitch);
    CHECK_CM_ERR(res);

    CmSurface2D* medata8_1;
    res = device->CreateSurface2D(medataParam8.numBytesInRow, medataParam8.numRows, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), medata8_1);
    CHECK_CM_ERR(res);
    res = medata8_1->WriteSurfaceStride(data8_1, NULL, data8_1Pitch);
    CHECK_CM_ERR(res);

    // 16x16
    CmSurface2D* medata16_0;
    res = device->CreateSurface2D(medataParam16.numBytesInRow, medataParam16.numRows, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), medata16_0);
    CHECK_CM_ERR(res);
    res = medata16_0->WriteSurfaceStride(data16_0, NULL, data16_0Pitch);
    CHECK_CM_ERR(res);

    CmSurface2D* medata16_1;
    res = device->CreateSurface2D(medataParam16.numBytesInRow, medataParam16.numRows, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), medata16_1);
    CHECK_CM_ERR(res);
    res = medata16_1->WriteSurfaceStride(data16_1, NULL, data16_1Pitch);
    CHECK_CM_ERR(res);

    // out predictors
    //int32_t sizeofPred = (width+63)/64 * (height+63)/64 * (64*64);
    int32_t widthInCtbs  = (width + MaxCUSize - 1) / MaxCUSize;
    int32_t heightInCtbs = (height+ MaxCUSize - 1) / MaxCUSize;
    int32_t numCtbs = widthInCtbs * heightInCtbs;
    int32_t sizeofPred = numCtbs * 64 * 64;

    CmSurface2D *outPredCm8 = 0;
    res = device->CreateSurface2D(widthInCtbs * 64, heightInCtbs * 64, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), outPredCm8);
    CHECK_CM_ERR(res);

    CmSurface2D *outPredCm16 = 0;
    res = device->CreateSurface2D(widthInCtbs * 64, heightInCtbs * 64, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), outPredCm16);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInSrc = 0;
    res = inSrc->GetIndex(idxInSrc);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInRef0 = 0;
    res = inRef0->GetIndex(idxInRef0);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInRef1 = 0;
    res = inRef1->GetIndex(idxInRef1);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxMeData8_0 = 0;
    res = medata8_0->GetIndex(idxMeData8_0);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxMeData8_1 = 0;
    res = medata8_1->GetIndex(idxMeData8_1);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxMeData16_0 = 0;
    res = medata16_0->GetIndex(idxMeData16_0);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxMeData16_1 = 0;
    res = medata16_1->GetIndex(idxMeData16_1);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutPred8 = 0;
    res = outPredCm8->GetIndex(idxOutPred8);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutPred16 = 0;
    res = outPredCm16->GetIndex(idxOutPred16);
    CHECK_CM_ERR(res);


    const mfxU16 BlockW = 16;
    const mfxU16 BlockH = BlockW;
    mfxU32 tsWidth = (width + BlockW - 1) / BlockW;
    mfxU32 tsHeight = (height + BlockH - 1) / BlockH;

    CmThreadSpace * threadSpace = 0;
    CmThreadGroupSpace* pTGS = NULL;
    if (isTGMode == 0) {
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);
        res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
        CHECK_CM_ERR(res);
        res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
        CHECK_CM_ERR(res);
    } else {
        int groupWidth   = (width + BlockW - 1) / BlockW;
        int groupHeight  = (height + BlockH - 1) / BlockH;
        tsWidth = 4;
        tsHeight= 4;
        int threadCount = (groupWidth * groupHeight) * (tsWidth * tsHeight);
        res = kernel->SetThreadCount(threadCount);
        CHECK_CM_ERR(res);
        res = device->CreateThreadGroupSpace(tsWidth, tsHeight, groupWidth, groupHeight, pTGS);
        CHECK_CM_ERR(res);

        device->SetSuggestedL3Config(SKL_L3_PLANE_5);
    }

    res = kernel->SetKernelArg(0, sizeof(SurfaceIndex), idxInSrc);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(SurfaceIndex), idxInRef0);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(SurfaceIndex), idxInRef1);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(3, sizeof(SurfaceIndex), idxMeData8_0);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(4, sizeof(SurfaceIndex), idxMeData8_1);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(5, sizeof(SurfaceIndex), idxMeData16_0);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(6, sizeof(SurfaceIndex), idxMeData16_1);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(7, sizeof(SurfaceIndex), idxOutPred8);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(8, sizeof(SurfaceIndex), idxOutPred16);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(9, sizeof(uint32_t), &refConfig);
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);
    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    CmEvent * e = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    //res = queue->Enqueue(task, e, threadSpace);
    res = isTGMode ? queue->EnqueueWithGroup(task, e, pTGS) : queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    // copy out data
    res = medata8_0->ReadSurfaceStride(data8_0, NULL, data8_0Pitch);
    CHECK_CM_ERR(res);
    res = medata8_1->ReadSurfaceStride(data8_1, NULL, data8_1Pitch);
    CHECK_CM_ERR(res);
    res = outPredCm8->ReadSurface(outPred8, NULL);

    res = medata16_0->ReadSurfaceStride(data16_0, NULL, data16_0Pitch);
    CHECK_CM_ERR(res);
    res = medata16_1->ReadSurfaceStride(data16_1, NULL, data16_1Pitch);
    CHECK_CM_ERR(res);
    res = outPredCm16->ReadSurface(outPred16, NULL);

    //device->FlushPrintBuffer();

#if !defined CMRT_EMU
    if (isTGMode)
        printf("TIME = %.3f ms ",  GetAccurateGpuTime_ThreadGroup(queue, task, pTGS) / 1000000.0);
    else
        printf("TIME = %.3f ms ",  GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif

    device->DestroyTask(task);


    res = device->DestroySurface(outPredCm16);
    res = device->DestroySurface(outPredCm8);
    device->DestroySurface(medata8_0);
    device->DestroySurface(medata8_1);
    device->DestroySurface(medata16_0);
    device->DestroySurface(medata16_1);

    device->DestroySurface(inRef1);
    device->DestroySurface(inRef0);
    device->DestroySurface(inSrc);

    if (isTGMode)
        device->DestroyThreadGroupSpace(pTGS);
    else
        device->DestroyThreadSpace(threadSpace);

    device->DestroyKernel(kernel);
    device->DestroyProgram(program);

    ::DestroyCmDevice(device);

    return PASSED;
}

int ReadFromFile(FILE* f, uint8_t* src, int32_t pitch, int32_t width, int32_t height)
{
    for (int32_t y = 0; y < height; y++) {
        size_t bytesRead = fread(src + y * pitch, 1, width, f);
        if (bytesRead != width)
            return FAILED;
    }

    return 0;
}

void PadOnePix(uint8_t *frame, int32_t pitch, int32_t width, int32_t height, int32_t padding)
{
    uint8_t *ptr = frame;
    for (int32_t i = 0; i < height; i++, ptr += pitch) {

        for (int32_t x = 1; x < padding; x++) {
                ptr[-x] = ptr[0];
                ptr[width - 1 + x] = ptr[width - 1];
            }
    }

    for (int32_t x = 1; x < padding; x++) {
        ptr = frame - padding;
        memcpy(ptr - x*pitch, ptr, pitch);
    }

    for (int32_t x = 1; x < padding; x++) {
        ptr = frame + (height - 1 + x) * pitch - padding;
        memcpy(ptr, ptr - pitch, pitch);
    }
}

namespace global_test_params {
    char *yuv = "C:/yuv/1080p/park_joy_1920x1080_500_50.yuv";
    int32_t width  = 1920;
    int32_t height = 1080;
};

void parse_cmd(int argc, char **argv) {
    if (argc == 2) {
        if (_stricmp(argv[1], "--help") == 0 ||
            _stricmp(argv[1], "-h") == 0 ||
            _stricmp(argv[1], "-?") == 0)
        {

            char exe_name[256], exe_ext[256];
            _splitpath_s(argv[0], nullptr, 0, nullptr, 0, exe_name, 256, exe_ext, 256);
            printf("Usage examples:\n\n");
            printf("  1) Run with default parameters (yuv=%s width=%d height=%d)\n",
                global_test_params::yuv, global_test_params::width, global_test_params::height);
            printf("    %s%s\n\n", exe_name, exe_ext);
            printf("  2) Run with custom parameters\n");
            printf("    %s%s <yuvname> <width> <height>\n", exe_name, exe_ext);
            exit(0);
        }
    }

    if (argc == 4) {
        global_test_params::yuv    = argv[1];
        global_test_params::width  = atoi(argv[2]);
        global_test_params::height = atoi(argv[3]);
    }
}

int main(int argc, char **argv)
{
    parse_cmd(argc, argv);
    print_hw_caps();

    printf("Running with %s %dx%d\n", global_test_params::yuv, global_test_params::width, global_test_params::height);

    const int32_t width = global_test_params::width;
    const int32_t height = global_test_params::height;

    FILE *f = fopen(global_test_params::yuv, "rb");
    if (!f)
        return FAILED;

    // const per frame params
    int32_t MaxCUSize = 64;
    int32_t picWidthInCtbs = (width + MaxCUSize - 1) / MaxCUSize;
    int32_t PAD = 96;

    // input data (src, 2 references, MeData)
    int32_t frameSize = 3 * (width * height) >> 1;
    int32_t frameSizePadded = 3 * ((width+2*PAD) * (height+2*PAD)) >> 1;

    std::vector<uint8_t> src_orig(frameSizePadded);
    uint8_t *src = src_orig.data() + PAD + PAD * (width+2*PAD);
    int32_t srcPitch = width + 2*PAD;
    int sts = ReadFromFile(f, src, srcPitch, width, height);
    CHECK_ERR(sts);
    PadOnePix(src, srcPitch, width, height, PAD);

    std::vector<uint8_t> ref0_orig(frameSizePadded);
    uint8_t *ref0 = ref0_orig.data() + PAD + PAD * (width+2*PAD);
    int32_t ref0Pitch = width + 2*PAD;
    sts = ReadFromFile(f, ref0, ref0Pitch, width, height);
    CHECK_ERR(sts);
    PadOnePix(ref0, ref0Pitch, width, height, PAD);

    std::vector<uint8_t> ref1_orig(frameSizePadded);
    uint8_t *ref1 = ref1_orig.data() + PAD + PAD * (width+2*PAD);
    int32_t ref1Pitch = width + 2*PAD;
    sts = ReadFromFile(f, ref1, ref1Pitch, width, height);
    CHECK_ERR(sts);
    PadOnePix(ref1, ref1Pitch, width, height, PAD);

    fclose(f);

    // format of output data (pix predictors) is set of 64x64 blocks (pitch == 64) instead of frame based.
    int32_t widthInCtbs  = (width + MaxCUSize - 1) / MaxCUSize;
    int32_t heightInCtbs = (height+ MaxCUSize - 1) / MaxCUSize;
    int32_t outputPitch = widthInCtbs * 64;
    std::vector<uint8_t> outputCpu(widthInCtbs * 64 * heightInCtbs * 64);
    std::vector<uint8_t> outputGpu(widthInCtbs * 64 * heightInCtbs * 64);

    // Simulation of medata
    mfxSurfInfoENC medataParam[4];
    GetSurfaceDimensions(width, height, medataParam);

    std::vector<uint8_t> medata[2][4];
    rand16u localrand;
    localrand.init(12345);

    for (int32_t ref = 0; ref < 2; ref++) {
        for (int32_t blksize = 0; blksize < 4; blksize++) {
            medata[ref][blksize].resize(medataParam[blksize].size);
        }
    }

    int32_t res;

    enum {CODEC_VP9 = 0, CODEC_AV1 = 1,};

    //for (int32_t codecType = CODEC_VP9; codecType <= CODEC_AV1; codecType++)
    int32_t codecType = CODEC_AV1;
    {
        const uint32_t isAV1 = (codecType == CODEC_AV1);
        for (int32_t blksize = 2; blksize < 4; blksize++) {
            for (int32_t refConfig = SINGLE_REF; refConfig <= COMPOUND; refConfig++) {
                FillRand(localrand, medata[0][blksize].data(), medataParam + blksize, blksize);
                FillRand(localrand, medata[1][blksize].data(), medataParam + blksize, blksize);

                // for one of the Kernel (say CPU) we need copy of meata0/1 because of rewritting it by the kernel
                std::vector<uint8_t> cpuCopyOfMeData0(medata[0][blksize]), cpuCopyOfMeData1(medata[1][blksize]);

                int32_t dataPitch = medataParam[blksize].pitch;

                printf("Testing %dx%d refConfig=%d ", 8 << blksize, 8 << blksize, refConfig);

                RunCpu(width, height, MaxCUSize,
                    blksize,
                    src, srcPitch,
                    ref0, ref0Pitch,
                    cpuCopyOfMeData0.data(), dataPitch,
                    ref1, ref1Pitch,
                    cpuCopyOfMeData1.data(), dataPitch,
                    outputCpu.data(), outputPitch,
                    refConfig,
                    isAV1);

                RunGpu(width, height, MaxCUSize,
                    blksize,
                    src, srcPitch,
                    ref0, ref0Pitch,
                    medata[0][blksize].data(), dataPitch,
                    ref1, ref1Pitch,
                    medata[1][blksize].data(), dataPitch,
                    outputGpu.data(),
                    refConfig,
                    medataParam[blksize],
                    isAV1);
#if NEWMVPRED
                res = Compare(outputGpu.data(), outputCpu.data(), width, height, blksize);
                CHECK_ERR(res);
#endif
                res = CompareBestIdx(cpuCopyOfMeData0.data(), dataPitch, medata[0][blksize].data(), dataPitch, width, height, blksize);
                CHECK_ERR(res);
                printf("\n");
            }
        }
    }

    //return printf("passed\n"), 0;

    //--------------------------------------------------------
    //    Combine Kernels testing
    //--------------------------------------------------------

    // 8x8 and 16x16
    std::vector<uint8_t> outputCpu8(widthInCtbs * 64 * heightInCtbs * 64);
    std::vector<uint8_t> outputCpu16(widthInCtbs * 64 * heightInCtbs * 64);
    std::vector<uint8_t> outputGpu8(widthInCtbs * 64 * heightInCtbs * 64);
    std::vector<uint8_t> outputGpu16(widthInCtbs * 64 * heightInCtbs * 64);
    codecType = CODEC_AV1;
    {
        uint32_t isAV1 = (codecType == CODEC_AV1);
        for (int32_t refConfig = SINGLE_REF; refConfig <= COMPOUND; refConfig++) {
            int32_t blksize = MFX_BLK_8x8;
            FillRand(localrand, medata[0][blksize].data(), medataParam + blksize, blksize);
            FillRand(localrand, medata[1][blksize].data(), medataParam + blksize, blksize);
            std::vector<uint8_t> cpuCopyOfMeData8_0(medata[0][blksize]), cpuCopyOfMeData8_1(medata[1][blksize]);

            printf("Testing 8x8 and 16x16 refConfig=%d ", refConfig);
            RunCpu(width, height, MaxCUSize,
                blksize,
                src, srcPitch,
                ref0, ref0Pitch,
                cpuCopyOfMeData8_0.data(), medataParam[blksize].pitch,
                ref1, ref1Pitch,
                cpuCopyOfMeData8_1.data(), medataParam[blksize].pitch,
                outputCpu8.data(), outputPitch,
                refConfig,
                isAV1);

            blksize = MFX_BLK_16x16;
            FillRand(localrand, medata[0][blksize].data(), medataParam + blksize, blksize);
            FillRand(localrand, medata[1][blksize].data(), medataParam + blksize, blksize);
            std::vector<uint8_t> cpuCopyOfMeData16_0(medata[0][blksize]), cpuCopyOfMeData16_1(medata[1][blksize]);
            RunCpu(width, height, MaxCUSize,
                blksize,
                src, srcPitch,
                ref0, ref0Pitch,
                cpuCopyOfMeData16_0.data(), medataParam[blksize].pitch,
                ref1, ref1Pitch,
                cpuCopyOfMeData16_1.data(), medataParam[blksize].pitch,
                outputCpu16.data(), outputPitch,
                refConfig,
                isAV1);


            RunGpuCombine8x16(width, height, MaxCUSize,
                src, srcPitch,
                ref0, ref0Pitch,
                medata[0][MFX_BLK_8x8].data(), medataParam[MFX_BLK_8x8].pitch,
                medata[0][MFX_BLK_16x16].data(), medataParam[MFX_BLK_16x16].pitch,
                ref1, ref1Pitch,
                medata[1][MFX_BLK_8x8].data(), medataParam[MFX_BLK_8x8].pitch,
                medata[1][MFX_BLK_16x16].data(), medataParam[MFX_BLK_16x16].pitch,
                outputGpu8.data(),
                outputGpu16.data(),
                refConfig,
                medataParam[MFX_BLK_8x8],
                medataParam[MFX_BLK_16x16],
                isAV1);

            res = Compare(outputGpu8.data(), outputCpu8.data(), width, height, MFX_BLK_8x8);
            CHECK_ERR(res);
            res = CompareBestIdx(cpuCopyOfMeData8_0.data(), medataParam[MFX_BLK_8x8].pitch, medata[0][MFX_BLK_8x8].data(), medataParam[MFX_BLK_8x8].pitch, width, height, MFX_BLK_8x8);
            CHECK_ERR(res);

            res = Compare(outputGpu16.data(), outputCpu16.data(), width, height, MFX_BLK_16x16);
            CHECK_ERR(res);
            res = CompareBestIdx(cpuCopyOfMeData16_0.data(), medataParam[MFX_BLK_16x16].pitch, medata[0][MFX_BLK_16x16].data(), medataParam[MFX_BLK_16x16].pitch, width, height, MFX_BLK_16x16);
            CHECK_ERR(res);

            printf("\n");
        }
    }

    return printf("passed\n"), 0;
}

/* EOF */
