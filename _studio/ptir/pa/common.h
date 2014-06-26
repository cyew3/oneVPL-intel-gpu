/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: common.h

\* ****************************************************************************** */
#ifndef PA_COMMON_H
#define PA_COMMON_H

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include <malloc.h>
#if defined(_WIN32) || defined(_WIN64)
#include <intrin.h>
#endif
#include <memory.h>
#include <math.h>
#include <stdio.h>

#if (defined(LINUX32) || defined(LINUX64))
#include <stdbool.h>
#include <inttypes.h>
#define __int64 uint64_t
#endif


#ifdef __cplusplus
extern "C" {
#endif

#define PA_C                    0
#define PA_SSE2                    1
#define PRINTX                    1

#define PA_PERF_LEVEL            PA_SSE2

#if defined(_WIN32) || defined(_WIN64)
#define USE_SSE4
#endif

#if defined(_WIN32) || defined(_WIN64)
#define ALIGN_DECL(X) __declspec(align(X))
#else
#define ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif

    // Latch
        typedef struct _Latch
    {
            BOOL 
                ucFullLatch,
                ucThalfLatch,
                ucSHalfLatch,
                ucParity;                //0 TFF, 1 BFF
    } Latch;

    // Pattern
        typedef struct _Pattern
    {
            BOOL 
                ucPatternFound;
            unsigned int
                ucPatternType,
                uiIFlush,
                uiPFlush,
                uiInterlacedFramesNum;
            double
                ucAvgSAD;
            Latch
                ucLatch;
            double
                blendedCount;
    } Pattern;

    //stats
        typedef struct _Stats
    {
            double ucSAD[5],
                ucRs[12];
    } Stats;

    // pla
        typedef struct _Plane
    {
            unsigned char *ucData;
            unsigned int uiSize;

            unsigned char *ucCorner;

            unsigned int uiWidth;
            unsigned int uiHeight;
            unsigned int uiStride;
            unsigned int uiBorder;
            Stats ucStats;
    } Plane;

    // meta
        typedef struct _Meta
    {
            BOOL 
                processed,        //Indicates if frame has been processed, used for first two frames in the beginning of the run
                interlaced,        //Indicates if frame is interlaced or not
                drop,            //Indicates if frame has to be dropped
                parity,            //Indicates the parity for the 3:2 pulldown (0) TFF, (1) BFF;
                candidate;
            double fr;
            long double timestamp;
            unsigned int tindex;
    } Meta;

    // frm
        typedef struct _Frame
    {
            unsigned char *ucMem;
            unsigned int uiSize;

            Plane plaY;
            Plane plaU;
            Plane plaV;

            Meta frmProperties;

            void *inSurf, *outSurf;
            enum {
                OUT_UNCHANGED,
                OUT_PROCESSED,
                OUT_DROPPED
            } outState;
    } Frame;

    // fn
        typedef struct _FrameNode
    {
            Frame                *pfrmItem;
            struct _FrameNode    *pfnNext;
    } FrameNode;

    // fq
        typedef struct _FrameQueue
    {
            FrameNode    *pfrmHead;
            FrameNode    *pfrmTail;
            int            iCount;
    } FrameQueue;

    //void PrintOutputStats(HANDLE hStd, CONSOLE_SCREEN_BUFFER_INFO sbInfo, unsigned int uiFrame, unsigned int uiStart, unsigned int uiCount, unsigned int *uiProgress, unsigned int uiFrameOut, unsigned int uiTimer, FrameQueue fqIn, const char **cOperations, double *dTime);

    void Plane_Create(Plane *pplaIn, unsigned int uiWidth, unsigned int uiHeight, unsigned int uiBorder);
    void Plane_Release(Plane *pplaIn);

    void Frame_Create(Frame *pfrmIn, unsigned int uiYWidth, unsigned int uiYHeight, unsigned int uiUVWidth, unsigned int uiUVHeight, unsigned int uiBorder);
    void Frame_Release(Frame *pfrmIn);

    void FrameQueue_Initialize(FrameQueue *pfq);
    void FrameQueue_Add(FrameQueue *pfq, Frame *pfrm);
    Frame * FrameQueue_Get(FrameQueue *pfq);
    void FrameQueue_Release(FrameQueue *pfq);

    void AddBorders(Plane *pplaIn, Plane *pplaOut, unsigned int uiBorder);
    void TrimBorders(Plane *pplaIn, Plane *pplaOut);

    void Convert_to_I420(unsigned char *pucIn, Frame *pfrmOut, char *pcFormat, double uiFrameRate);
    void CheckGenFrame(Frame **pfrmIn, unsigned int frameNum, unsigned int patternType, unsigned int uiOPMode);
    void Prepare_frame_for_queue(Frame **pfrmOut, Frame *pfrmIn, unsigned int uiWidth, unsigned int uiHeight);
#ifdef __cplusplus
}
#endif 
#endif