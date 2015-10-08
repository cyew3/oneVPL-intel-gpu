/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
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

const mfxI32 WIDTH  = 1920;
const mfxI32 HEIGHT = 1088;
const mfxI8 YUV_NAME[] = "C:/yuv/1080p/BasketballDrive_1920x1080p_500_50.yuv";
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
            queue->EnqueueCopyCPUToGPUStride(copyParam.gpumem, copyParam.cpumem, copyParam.pitch, e1);
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
struct Me2xControl // sizeof=64
{
    VmeSearchPath searchPath;
    mfxU8  reserved[2];
    mfxU16 width;
    mfxU16 height;
};

inline void SetupMeControl(Me2xControl &ctrl, int width, int height)
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

inline void SetupMeControlShortPath(Me2xControl &ctrl, int width, int height)
{
    const mfxU8 SmallPath[3] = { 0x0F, 0xF0, 0x01 };
    memcpy(ctrl.searchPath.sp, SmallPath, sizeof(SmallPath));
    ctrl.searchPath.lenSp = 4;
    ctrl.searchPath.maxNumSu = 9;
    ctrl.width = (mfxU16)width;
    ctrl.height = (mfxU16)height;
}
