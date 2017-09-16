/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "tuple"

typedef char mfxI8;
typedef char Ipp8s;
typedef unsigned char mfxU8;
typedef unsigned char Ipp8u;
typedef short mfxI16;
typedef short Ipp16s;
typedef unsigned short mfxU16;
typedef unsigned short Ipp16u;
typedef int mfxI32;
typedef int Ipp32s;
typedef unsigned int mfxU32;
typedef unsigned int Ipp32u;
typedef __int64 mfxI64;
typedef __int64 Ipp64s;
typedef unsigned __int64 mfxU64;
typedef unsigned __int64 Ipp64u;
typedef double mfx64F;
typedef double Ipp64f;
typedef float mfx32F;
typedef float Ipp32f;

typedef struct {
    mfxI16  x;
    mfxI16  y;
} mfxI16Pair;

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ABS(a)    (((a) < 0) ? (-(a)) : (a))

#define CLIPVAL(VAL, MINVAL, MAXVAL) MAX(MINVAL, MIN(MAXVAL, VAL))
#define CHECK_CM_ERR(ERR) if ((ERR) != CM_SUCCESS) { printf("FAILED at file: %s, line: %d, cmerr: %d\n", __FILE__, __LINE__, ERR); return FAILED; }
#define CHECK_ERR(ERR) if ((ERR) != PASSED) { printf("FAILED at file: %s, line: %d\n", __FILE__, __LINE__); return (ERR); }
#define DIVUP(a, b) ((a+b-1)/b)
#define ROUNDUP(a, b) DIVUP(a,b)*b


#define BORDER          4
#define WIDTHB (WIDTH + BORDER*2)
#define HEIGHTB (HEIGHT + BORDER*2)

enum { PASSED, FAILED };

//#define TEST_8K
#ifdef TEST_8K
const mfxI32 WIDTH  = 8192;
const mfxI32 HEIGHT = 4096;
const mfxI8 YUV_NAME[] = "C:/yuv/8K/rally_8192x4096_30.yuv";
#else
const mfxI32 WIDTH  = 1920;
const mfxI32 HEIGHT = 1088;
const mfxI8 YUV_NAME[] = "C:/yuv/1080p/BasketballDrive_1920x1080p_500_50.yuv";
#endif
//const mfxI32 WIDTH  = 416;
//const mfxI32 HEIGHT = 240;
//const mfxI8 YUV_NAME[] = "C:/yuv/JCTVC-G1200/240p/RaceHorses_416x240p_300_30.yuv";

struct ParamCopyCPUToGPUStride {
    ParamCopyCPUToGPUStride(CmSurface2D *gpumem_, const Ipp8u *cpumem_, Ipp32s pitch_) : gpumem(gpumem_), cpumem(cpumem_), pitch(pitch_) {}
    CmSurface2D *gpumem;
    const Ipp8u *cpumem;
    Ipp32s       pitch;
};

inline mfxU64 GetAccurateGpuTime(CmQueue *queue, CmTask *task, CmThreadSpace *ts)
{
    CmEvent *e = NULL;
    mfxU64 mintime = mfxU64(-1);
    for (int i=0; i<10; i++) {
        for (int j=0; j<10; j++)
            queue->Enqueue(task, e, ts);
        e->WaitForTaskFinished();
        mfxU64 time;
        e->GetExecutionTime(time);
        mintime = MIN(mintime, time);
    }
    return mintime;
}

inline std::tuple<mfxU64,mfxU64,mfxU64> GetAccurateGpuTime(CmQueue *queue, const ParamCopyCPUToGPUStride &copyParam, CmTask *task, CmThreadSpace *ts)
{
    CmEvent *e1 = NULL, *e2 = NULL;
    mfxU64 mintimeTot = mfxU64(-1);
    mfxU64 mintime1 = mfxU64(-1);
    mfxU64 mintime2 = mfxU64(-1);
    for (int i=0; i<10; i++) {
        for (int j=0; j<10; j++) {
            queue->EnqueueCopyCPUToGPUFullStride(copyParam.gpumem, copyParam.cpumem, copyParam.pitch, 0, 0, e1);
            queue->Enqueue(task, e2, ts);
        }
        e2->WaitForTaskFinished();
        mfxU64 time1, time2;
        e1->GetExecutionTime(time1);
        e2->GetExecutionTime(time2);
        if (mintimeTot > time1 + time2) {
            mintimeTot = time1 + time2;
            mintime1 = time1;
            mintime2 = time2;
        }
    }
    return std::make_tuple(mintime1, mintime2, mintimeTot);
}

inline mfxU64 GetAccurateGpuTime_ThreadGroup(CmQueue *queue, CmTask *task, CmThreadGroupSpace* tgs)
{
    CmEvent *e = NULL;
    mfxU64 mintime = mfxU64(-1);
    for (int i=0; i<10; i++) {
        for (int j=0; j<10; j++)
            queue->EnqueueWithGroup(task, e, tgs);
        e->WaitForTaskFinished();
        mfxU64 time;
        e->GetExecutionTime(time);
        mintime = MIN(mintime, time);
    }
    return mintime;
}

struct VmeSearchPath // sizeof=58
{
    mfxU8 sp[56];
    mfxU8 maxNumSu;
    mfxU8 lenSp;
};

struct Me2xControl_old // sizeof=64
{
    VmeSearchPath searchPath;
    mfxU8  reserved[2];
    mfxU16 width;
    mfxU16 height;
};

struct MeControl // sizeof=120
{
    mfxU8  longSp[32];          // 0 B
    mfxU8  shortSp[32];         // 32 B
    mfxU8  longSpLenSp;         // 64 B
    mfxU8  longSpMaxNumSu;      // 65 B
    mfxU8  shortSpLenSp;        // 66 B
    mfxU8  shortSpMaxNumSu;     // 67 B
    mfxU16 width;               // 34 W
    mfxU16 height;              // 35 W
    mfxU8  mvCost[5][8];         // 72 B (1x), 80 B (2x), 88 B (4x), 96 B (8x) 104 B (16x)
    mfxU8  mvCostScaleFactor[5]; // 112 B (1x), 113 B (2x), 114 B (4x), 115 B (8x) 116 B (16x)
    mfxU8  reserved[3];          // 117 B
};

inline void SetupMeControl_old(Me2xControl_old &ctrl, int width, int height)
{
    const mfxU8 Diamond[56] =
    {
        0x0F,0xF1,0x0F,0x12,//5
        0x0D,0xE2,0x22,0x1E,//9
        0x10,0xFF,0xE2,0x20,//13
        0xFC,0x06,0xDD,//16
        0x2E,0xF1,0x3F,0xD3,0x11,0x3D,0xF3,0x1F,//24
        0xEB,0xF1,0xF1,0xF1,//28
        0x4E,0x11,0x12,0xF2,0xF1,//33
        0xE0,0xFF,0xFF,0x0D,0x1F,0x1F,//39
        0x20,0x11,0xCF,0xF1,0x05,0x11,//45
        0x00,0x00,0x00,0x00,0x00,0x00,//51
    };

    memcpy(ctrl.searchPath.sp, Diamond, sizeof(ctrl.searchPath));
    ctrl.searchPath.lenSp = 16;
    ctrl.searchPath.maxNumSu = 57;
    ctrl.width = (mfxU16)width;
    ctrl.height = (mfxU16)height;
}

inline void SetupMeControlShortPath_old(Me2xControl_old &ctrl, int width, int height)
{
    const mfxU8 SmallPath[3] = { 0x0F, 0xF0, 0x01 };
    memcpy(ctrl.searchPath.sp, SmallPath, sizeof(SmallPath));
    ctrl.searchPath.lenSp = 4;
    ctrl.searchPath.maxNumSu = 9;
    ctrl.width = (mfxU16)width;
    ctrl.height = (mfxU16)height;
}


inline mfxU8 ToU4U4(mfxU16 val) {
    if (val > 4095)
        val = 4095;
    mfxU16 shift = 0;
    mfxU16 base = val;
    mfxU16 rem = 0;
    while (base > 15) {
        rem += (base & 1) << shift;
        base = (base >> 1);
        shift++;
    }
    base += (rem << 1 >> shift);
    return base | (shift << 4);
}


inline void SetupMeControl(MeControl &ctrl, int width, int height, double lambda)
{
    const mfxU8 Diamond[56] = {
        0x0F,0xF1,0x0F,0x12,//5
        0x0D,0xE2,0x22,0x1E,//9
        0x10,0xFF,0xE2,0x20,//13
        0xFC,0x06,0xDD,//16
        0x2E,0xF1,0x3F,0xD3,0x11,0x3D,0xF3,0x1F,//24
        0xEB,0xF1,0xF1,0xF1,//28
        0x4E,0x11,0x12,0xF2,0xF1,//33
        0xE0,0xFF,0xFF,0x0D,0x1F,0x1F,//39
        0x20,0x11,0xCF,0xF1,0x05,0x11,//45
        0x00,0x00,0x00,0x00,0x00,0x00,//51
    };

    memcpy(ctrl.longSp, Diamond, MIN(sizeof(Diamond), sizeof(ctrl.longSp)));
    ctrl.longSpLenSp = 16;
    ctrl.longSpMaxNumSu = 57;

    const mfxU8 ShortPath[4] = {
        0x0F, 0xF0, 0x01, 0x00
    };

    memcpy(ctrl.shortSp, ShortPath, MIN(sizeof(ShortPath), sizeof(ctrl.shortSp)));
    ctrl.shortSpLenSp = 4;
    ctrl.shortSpMaxNumSu = 9;

    ctrl.width = (mfxU16)width;
    ctrl.height = (mfxU16)height;
    
    const mfxU8 MvBits[4][8] = { // mvCostScale = qpel, hpel, ipel 2pel
        { 1, 4, 5, 6, 8, 10, 12, 14 },
        { 1, 5, 6, 8, 10, 12, 14, 16 },
        { 1, 6, 8, 10, 12, 14, 16, 18 },
        { 1, 8, 10, 12, 14, 16, 18, 20 }
    };

    ctrl.mvCostScaleFactor[0] = 2; // int-pel cost table precision
    ctrl.mvCostScaleFactor[1] = 2;
    ctrl.mvCostScaleFactor[2] = 2;
    ctrl.mvCostScaleFactor[3] = 2;
    ctrl.mvCostScaleFactor[4] = 2;

    const mfxU8 *mvBits = MvBits[ctrl.mvCostScaleFactor[0]];
    for (Ipp32s i = 0; i < 8; i++)
        ctrl.mvCost[0][i] = ToU4U4(mfxU16(0.5 + lambda * mvBits[i]));

    //mvBits = MvBits[ctrl.mvCostScaleFactor[1]];
    //for (Ipp32s i = 0; i < 8; i++)
    //    ctrl.mvCost[1][i] = ToU4U4(mfxU16(0.5 + lambda /  4 * (1 * (i > 0) + mvBits[i])));

    //mvBits = MvBits[ctrl.mvCostScaleFactor[2]];
    //for (Ipp32s i = 0; i < 8; i++)
    //    ctrl.mvCost[2][i] = ToU4U4(mfxU16(0.5 + lambda / 16 * (2 * (i > 0) + mvBits[i])));

    //mvBits = MvBits[ctrl.mvCostScaleFactor[3]];
    //for (Ipp32s i = 0; i < 8; i++)
    //    ctrl.mvCost[3][i] = ToU4U4(mfxU16(0.5 + lambda / 64 * (3 * (i > 0) + mvBits[i])));

    //mvBits = MvBits[ctrl.mvCostScaleFactor[4]];
    //for (Ipp32s i = 0; i < 8; i++)
    //    ctrl.mvCost[4][i] = ToU4U4(mfxU16(0.5 + lambda / 64 * (4 * (i > 0) + mvBits[i])));

    memset(ctrl.mvCost, 0, sizeof(ctrl.mvCost));

    mfxU8 MvCostHumanFriendly[5][8];
    for (Ipp32s i = 0; i < 5; i++)
        for (Ipp32s j = 0; j < 8; j++)
            MvCostHumanFriendly[i][j] = (ctrl.mvCost[i][j] & 0xf) << ((ctrl.mvCost[i][j] >> 4) & 0xf);
}

inline int Dump(const Ipp8u* data, size_t frameSize, const char* fileName)
{
#ifdef _MSC_VER
    FILE *fout = NULL;
    fopen_s(&fout, fileName, "wb");
#else
    FILE *fout = fopen(fileName, "wb");
#endif
    if (!fout) {
        return FAILED;
    }

    fwrite(data, 1, frameSize, fout);
    fclose(fout);

    return PASSED;
}

inline void* Read(const char* fileName, Ipp32u* fileSize)
{
#ifdef _MSC_VER
    FILE *fin = NULL;
    fopen_s(&fin, fileName, "rb");
#else
    FILE *fin = fopen(fileName, "rb");
#endif
    if (!fin) {
        fprintf(stderr, "Error opening %s\n", fileName);
        return NULL;
    }

    fseek (fin , 0 , SEEK_END);
    *fileSize = ftell (fin);
    rewind (fin);

    void *content = malloc(*fileSize);
    if (!content) {
        fprintf(stderr, "Failed to allocate %lu bytes for file content\n", *fileSize);
        return NULL;
    }

    size_t read_cnt = fread(content, 1, *fileSize, fin);
    fclose(fin);

    if (read_cnt != *fileSize) {
        fprintf(stderr, "Error reading %lu bytes from %s\n", *fileSize, fileName);
        free(content);
        return NULL;
    }

    return content;
}

// Copy from (smaller) unaligned picture to bigger (aligned) one;
inline int CopyToAlignedNV12(
	Ipp8u*	&dst,
	Ipp8u*	src,
	Ipp32u	dst_w,
	Ipp32u	dst_h,
	Ipp32u	src_w,
	Ipp32u	src_h)
{
	if( !src || (dst_w < src_w) || (dst_h < src_h) )
		return 1;

	if(!dst)
		dst = (Ipp8u*)calloc(dst_h * dst_w * 3 / 2, sizeof(Ipp8u));

	if(!dst)
		return 1;

	Ipp8u *psrc = src;
	Ipp8u *pdst = dst;

	// Luma
	for(Ipp32u y = 0U; y < src_h; y++) {
		memcpy(pdst, psrc, src_w);
		psrc += src_w;
		pdst += dst_w;
	}

	// Chroma
	pdst = dst + dst_w * dst_h;
    psrc = src + src_w * src_h;
	for(Ipp32u y = 0U; y < src_h / 2; y++) {
		memcpy(pdst, psrc, src_w);
		psrc += src_w;
		pdst += dst_w;
	}

	return 0;
}

// Copy from (bigger) aligned NV12 picture to (smaller) unaligned one;
inline int CopyFromAlignedNV12(
	Ipp8u*	&dst,
	Ipp8u*	src,
	Ipp32u	dst_w,
	Ipp32u	dst_h,
	Ipp32u	src_w,
	Ipp32u	src_h)
{
	if( !src || (dst_w > src_w) || (dst_h > src_h) )
		return 1;

	if(!dst)
		dst = (Ipp8u*)calloc(dst_h * dst_w * 3 / 2, sizeof(Ipp8u));

	if(!dst)
		return 1;

	Ipp8u *psrc = src;
	Ipp8u *pdst = dst;

	// Luma
	for(Ipp32u y = 0U; y < dst_h; y++) {
		memcpy(pdst, psrc, dst_w);
		psrc += src_w;
		pdst += dst_w;
	}

	// Chroma
	pdst = dst + dst_w * dst_h;
    psrc = src + src_w * src_h;
	for(Ipp32u y = 0U; y < dst_h / 2; y++) {
		memcpy(pdst, psrc, dst_w);
		psrc += src_w;
		pdst += dst_w;
	}

	return 0;
}