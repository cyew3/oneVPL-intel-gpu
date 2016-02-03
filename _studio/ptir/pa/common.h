/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: common.h

\* ****************************************************************************** */
#ifndef PTIR_COMMON_H
#define PTIR_COMMON_H

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include <malloc.h>
#if defined(_WIN32) || defined(_WIN64)
#include <intrin.h>
#endif
#include <math.h>
#include <stdio.h>
#include "ptir_memory.h"

#if defined(_WIN32) || defined(_WIN64)

#include <Windows.h>
#include <malloc.h>
#include <intrin.h>
#define ALIGN_DECL(X) __declspec(align(X))
#else

#include <stdlib.h>

#ifdef __cplusplus
//typedef bool BOOL;
#else
//typedef _Bool BOOL;
#endif

#define    __stosb memset

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
#include <smmintrin.h>
//typedef long long __int64;
//typedef unsigned char BYTE;

#define ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (defined(LINUX32) || defined(LINUX64))
#include <stdbool.h>
#include <inttypes.h>
#ifndef __int64
#define __int64 uint64_t
#endif

    //Visual Studio C-compiler does not support C99, so that no bool type at all
    //Defining bool (in lower case) will cause a lot of errors in C++ files including this file
#define BOOL bool
#ifndef TRUE
#define TRUE true
#endif
#ifndef FALSE
#define FALSE false
#endif
#define BYTE unsigned char

//#include <emmintrin.h> //SSE2 
//#include <pmmintrin.h> //SSE3 (Prescott) Pentium(R) 4 processor (_mm_lddqu_si128() present ) 
//#include <tmmintrin.h> // SSSE3 
#include <smmintrin.h> // SSE4.1, Intel(R) Core(TM) 2 Duo
//#include <nmmintrin.h> // SSE4.2, Intel(R) Core(TM) 2 Duo
//#include <wmmintrin.h> // AES and PCLMULQDQ
//#include <immintrin.h> // AVX, AVX2
//#include <ammintrin.h> // AMD extention SSE5 ? FMA4

#endif

#define BUFMINSIZE               6
#define BUFEXTRASIZE             7
#define NEXTFIELD                6
#define PREVBUF                  7
#define RSTBUF                   8
#define TEMPBUF                  9
#define BADMCBUF                 10
#define EDGESBUF                 11
#define DIREDGEBUF               12
#define LASTFRAME                BUFMINSIZE + BUFEXTRASIZE

#define PA_C                     0
#define PA_SSE2                  1
#define PRINTDEBUG               0
#define PRINTXPERT               0

#define PA_PERF_LEVEL            PA_SSE2

#define USE_SSE4

#define READY                    0
#define NOTREADY                 1
#define AUTOMODE                 0
#define DEINTERLACEMODE          1
#define TELECINE24FPSMODE        2
#define BASEFRAMEMODE            3
#define FULLFRAMERATEMODE        1
#define HALFFRAMERATEMODE        0
#define MODE_NOT_AVAILABLE       -1
#define THOMSONMODE              0

// Latch
    typedef struct _Latch
    {
        BOOL ucFullLatch,
             ucThalfLatch,
             ucSHalfLatch,
             ucParity;                //0 TFF, 1 BFF for telecine content, it will not modify deinterlacing process
    } Latch;

    // Pattern
    typedef struct _Pattern
    {
        BOOL ucPatternFound;
        unsigned int ucPatternType,
                     uiIFlush,
                     uiPFlush,
                     uiInterlacedFramesNum,
                     uiOverrideHalfFrameRate,
                     uiCountFullyInterlacedBuffers,
                     uiPreviousPartialPattern,
                     uiPartialPattern;
        double ucAvgSAD;
        Latch  ucLatch;
        double blendedCount;
        unsigned long ulCountInterlacedDetections;
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
        BOOL processed,        //Indicates if frame has been processed, used for first two frames in the beginning of the run
             interlaced,       //Indicates if frame is interlaced or not
             detection,
             drop,             //Indicates if frame has to be dropped
             parity,           //Indicates the parity for the 3:2 pulldown (0) TFF, (1) BFF;
             drop24fps,
             sceneChange,
             candidate;
        double fr;
        double timestamp;
        unsigned int tindex;
        unsigned int uiInterlacedDetectionValue;
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
        Frame *pfrmItem;
        struct _FrameNode *pfnNext;
    } FrameNode;

    // fq
    typedef struct _FrameQueue
    {
        FrameNode *pfrmHead;
        FrameNode *pfrmTail;
        int iCount;
    } FrameQueue;

    typedef struct _PTIRBufCon
    {
        unsigned int uiCur;
        unsigned int uiNext;
        unsigned int uiFrame;
        unsigned int uiDispatch;
        unsigned int uiInterlaceParity;
        unsigned int uiInWidth;
        unsigned int uiInHeight;
        unsigned int uiWidth;
        unsigned int uiHeight;
        unsigned int uiSize;
        unsigned int uiBufferCount;
        unsigned int uiFrameCount;
        unsigned int uiEndOfFrames;
        unsigned int uiPatternTypeNumber;
        unsigned int uiPatternTypeInit;
        unsigned int uiFrameOut;
        double       dFrameRate,
                     dBaseTime,
                     dTimeStamp,
                     dDeIntTime,
                     dBaseTimeSw,
                     dTimePerFrame,
                     pts,
                     frame_duration;
        Pattern      mainPattern;

    } PTIRBufferControl;

    typedef struct _PTIRSys
    {
        Frame        *frmIO[LASTFRAME + 1],
                     *frmIn,
                     *frmBuffer[LASTFRAME];
        FrameQueue   fqIn;
        PTIRBufferControl control;
        void         *frmSupply;
    } PTIRSystemBuffer;

    typedef struct _pPTIRSys
    {
        PTIRSystemBuffer *SysBuffer;
    } pPTIRSystemBuffer;

    typedef struct _PTIREnvironment
    {
        PTIRSystemBuffer Env;
        void             (*PTIRModeFunction) ( pPTIRSystemBuffer);
    } PTIREnvironment;

#if defined(_WIN32) || defined(_WIN64)
    void   PrintOutputStats(HANDLE hStd, CONSOLE_SCREEN_BUFFER_INFO sbInfo, unsigned int uiFrame, unsigned int uiStart, unsigned int uiCount, unsigned int *uiProgress, unsigned int uiFrameOut, unsigned int uiTimer, FrameQueue fqIn, const char **cOperations, double *dTime);
#endif
    void   PrintOutputStats_PvsI(unsigned long ulNumberofInterlacedFrames, unsigned long ulTotalNumberofFramesProcessed, const char **cSummary);
    void*  aligned_malloc(size_t required_bytes, size_t alignment);
    void   aligned_free(void *p);
    void   Plane_Create(Plane *pplaIn, unsigned int uiWidth, unsigned int uiHeight, unsigned int uiBorder);
    void   Plane_Release(Plane *pplaIn);
    void   Frame_Create(Frame *pfrmIn, unsigned int uiYWidth, unsigned int uiYHeight, unsigned int uiUVWidth, unsigned int uiUVHeight, unsigned int uiBorder);
    void   Frame_Release(Frame *pfrmIn);
    void   FrameQueue_Initialize(FrameQueue *pfq);
    void   FrameQueue_Add(FrameQueue *pfq, Frame *pfrm);
    Frame* FrameQueue_Get(FrameQueue *pfq);
    void   FrameQueue_Release(FrameQueue *pfq);
    void   AddBorders(Plane *pplaIn, Plane *pplaOut, unsigned int uiBorder);
    void   TrimBorders(Plane *pplaIn, Plane *pplaOut);
    unsigned int   Convert_to_I420(unsigned char *pucIn, Frame *pfrmOut, char *pcFormat, double dFrameRate);
    void   Clean_Frame_Info(Frame **frmBuffer);
    void   Frame_Prep_and_Analysis(Frame **frmBuffer, char *pcFormat, double dFrameRate, unsigned int uiframeBufferIndexCur, unsigned int uiframeBufferIndexNext, unsigned int uiTemporalIndex);
    void   CheckGenFrame(Frame **pfrmIn, unsigned int frameNum, unsigned int uiOPMode);
    void   Prepare_frame_for_queue(Frame **pfrmOut, Frame *pfrmIn, unsigned int uiWidth, unsigned int uiHeight);
    double Calculate_Resulting_timestamps(Frame** frmBuffer, unsigned int uiDispatch, unsigned int uiCur, double dBaseTime, unsigned int *uiNumFramesToDispatch, unsigned int PatternType, unsigned int uiEndOfFrames);
    void   Update_Frame_Buffer(Frame** frmBuffer, unsigned int frameIndex, double dTimePerFrame, unsigned int uiisInterlaced, unsigned int uiInterlaceParity, unsigned int bFullFrameRate, Frame* frmIn, FrameQueue *fqIn);
    void   Update_Frame_BufferNEW(Frame** frmBuffer, unsigned int frameIndex, double dCurTimeStamp, double dTimePerFrame, unsigned int uiisInterlaced, unsigned int uiInterlaceParity, unsigned int bFullFrameRate, Frame* frmIn, FrameQueue *fqIn);

    /*PTIR Complete Functions*/
    int    PTIR_AutoMode_FF(PTIRSystemBuffer *SysBuffer); /*Detection and processing of progressive, telecined and interlaced content.
                                                                         System returns full frame rate from interlaced content. Intended for variable frame rate encoding*/
    int    PTIR_AutoMode_HF(PTIRSystemBuffer *SysBuffer); /*Detection and processing of progressive, telecined and interlaced content.
                                                                         System returns half frame rate from interlaced content. Intended for variable frame rate encoding*/
    int    PTIR_DeinterlaceMode_FF(PTIRSystemBuffer *SysBuffer); /*Direct deinterlacing without content analysis. System returns full frame rate from deinterlacing process.
                                                                                Constant frame rate*/
    int    PTIR_DeinterlaceMode_HF(PTIRSystemBuffer *SysBuffer); /*Direct deinterlacing without content analysis. System returns half frame rate from deinterlacing process.
                                                                                Constant frame rate*/
    int    PTIR_BaseFrameMode(PTIRSystemBuffer *SysBuffer); /*Detection and processing of progressive, telecined and interlaced content.
                                                                           System returns same input framerate. Constant frame rate*/
    int    PTIR_BaseFrameMode_NoChange(PTIRSystemBuffer *SysBuffer);
    int    PTIR_Auto24fpsMode(PTIRSystemBuffer *SysBuffer);
    int    PTIR_FixedTelecinePatternMode(PTIRSystemBuffer *SysBuffer);
    int    PTIR_MultipleMode(PTIRSystemBuffer *SysBuffer, unsigned int uiOpMode);
    int    PTIR_SimpleMode(PTIRSystemBuffer *SysBuffer, unsigned int uiOpMode);
    Frame* PTIR_GetFrame(PTIRSystemBuffer *SysBuffer);
    int    PTIR_PutFrame(unsigned char *pucIO, PTIRSystemBuffer *SysBuffer, double dTimestamp);
    void   PTIR_Frame_Prep_and_Analysis(PTIRSystemBuffer *SysBuffer);
    void   PTIR_Init(PTIRSystemBuffer *SysBuffer, unsigned int _uiInWidth, unsigned int _uiInHeight, double _dFrameRate, unsigned int _uiInterlaceParity, unsigned int _uiPatternTypeNumber, unsigned int _uiPatternTypeInit);
    void   PTIR_Clean(PTIRSystemBuffer *SysBuffer);
#ifdef __cplusplus
}
#endif 
#endif
