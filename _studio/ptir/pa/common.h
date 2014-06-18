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

//#ifdef PA_EXPORTS
//#define /*PA_API*/ __declspec(dllexport)
//#else
//#define /*PA_API*/ __declspec(dllimport)
//#endif

#include <Windows.h>
#include <malloc.h>
#include <intrin.h>
#include <memory.h>
#include <math.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PA_C                    0
#define PA_SSE2                    1
#define PRINTX                    1

#define PA_PERF_LEVEL            PA_SSE2

#define USE_SSE4

#if defined(_WIN32) || defined(_WIN64)
#define ALIGN_DECL(X) __declspec(align(X))
#else
#define ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif

    // Latch
    /*PA_API*/
        typedef struct _Latch
    {
            BOOL 
                ucFullLatch,
                ucThalfLatch,
                ucSHalfLatch,
                ucParity;                //0 TFF, 1 BFF
    } Latch;

    // Pattern
    /*PA_API*/
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
    /*PA_API*/
        typedef struct _Stats
    {
            double ucSAD[5],
                ucRs[12];
    } Stats;

    // pla
    /*PA_API*/
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
    /*PA_API*/
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
    /*PA_API*/
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
    /*PA_API*/
        typedef struct _FrameNode
    {
            Frame                *pfrmItem;
            struct _FrameNode    *pfnNext;
    } FrameNode;

    // fq
    /*PA_API*/
        typedef struct _FrameQueue
    {
            FrameNode    *pfrmHead;
            FrameNode    *pfrmTail;
            int            iCount;
    } FrameQueue;

    /*PA_API*/ void /*__stdcall*/ PrintOutputStats(HANDLE hStd, CONSOLE_SCREEN_BUFFER_INFO sbInfo, unsigned int uiFrame, unsigned int uiStart, unsigned int uiCount, unsigned int *uiProgress, unsigned int uiFrameOut, unsigned int uiTimer, FrameQueue fqIn, const char **cOperations, double *dTime);

    /*PA_API*/ void /*__stdcall*/ Plane_Create(Plane *pplaIn, unsigned int uiWidth, unsigned int uiHeight, unsigned int uiBorder);
    /*PA_API*/ void /*__stdcall*/ Plane_Release(Plane *pplaIn);

    /*PA_API*/ void /*__stdcall*/ Frame_Create(Frame *pfrmIn, unsigned int uiYWidth, unsigned int uiYHeight, unsigned int uiUVWidth, unsigned int uiUVHeight, unsigned int uiBorder);
    /*PA_API*/ void /*__stdcall*/ Frame_Release(Frame *pfrmIn);

    /*PA_API*/ void /*__stdcall*/ FrameQueue_Initialize(FrameQueue *pfq);
    /*PA_API*/ void /*__stdcall*/ FrameQueue_Add(FrameQueue *pfq, Frame *pfrm);
    /*PA_API*/ Frame * /*__stdcall*/ FrameQueue_Get(FrameQueue *pfq);
    /*PA_API*/ void /*__stdcall*/ FrameQueue_Release(FrameQueue *pfq);

    /*PA_API*/ void /*__stdcall*/ AddBorders(Plane *pplaIn, Plane *pplaOut, unsigned int uiBorder);
    /*PA_API*/ void /*__stdcall*/ TrimBorders(Plane *pplaIn, Plane *pplaOut);

    /*PA_API*/ void /*__stdcall*/ Convert_to_I420(unsigned char *pucIn, Frame *pfrmOut, char *pcFormat, double uiFrameRate);
    /*PA_API*/ void /*__stdcall*/ CheckGenFrame(Frame **pfrmIn, unsigned int frameNum, unsigned int patternType, unsigned int uiOPMode);
    /*PA_API*/ void /*__stdcall*/ Prepare_frame_for_queue(Frame **pfrmOut, Frame *pfrmIn, unsigned int uiWidth, unsigned int uiHeight);
#ifdef __cplusplus
}
#endif 
#endif