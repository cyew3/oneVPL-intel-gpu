//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//
#ifndef __ASC_DEFS__
#define __ASC_DEFS__

#define ASCTUNEDATA       0
#define ENABLE_RF         1

#define DUMP_Yonly_small_128x64 0

#undef  NULL
#define NULL              0
#define OUT_BLOCK         16  // output pixels computed per thread
#define MVBLK_SIZE        8
#define BLOCK_SIZE        4
#define BLOCK_SIZE_SHIFT  2
#define NumTSC            10
#define NumSC             10
#define FLOAT_MAX         2241178.0
#define FRAMEMUL          16
#define CHROMASUBSAMPLE   4
#define SMALL_WIDTH       128//112
#define SMALL_HEIGHT      64
#define LN2               0.6931471805599453
#define MEMALLOCERROR     1000
#define MEMALLOCERRORU8   1001
#define MEMALLOCERRORMV   1002
#define MAXLTRHISTORY     120
#define SMALL_AREA        8192//13 bits
#define S_AREA_SHIFT      13
#define TSC_INT_SCALE     5
#define GAINDIFF_THR      20

/*--MACROS--*/
#define NMAX(a,b)         ((a>b)?a:b)
#define NMIN(a,b)         ((a<b)?a:b)
#define NABS(a)           (((a)<0)?(-(a)):(a))
#define NAVG(a,b)         ((a+b)/2)

#define Clamp(x)           ((x<0)?0:((x>255)?255:x))
#define RF_DECISION_LEVEL 10
#define NEWFEATURE        0
#if NEWFEATURE
#define TSCSTATBUFFER     4
#define VIDEOSTATSBUF     4
#else
#define TSCSTATBUFFER     3
#define VIDEOSTATSBUF     2
#endif
#define SIMILITUDVAL      4
#define NODELAY           1 //No delay checks the resulting decision directly after the comparison has been done, no last frame in scene info or nest frame is a scene change
#define SCD_BLOCK_PIXEL_WIDTH   32
#define SCD_BLOCK_HEIGHT        8

#define EXTRANEIGHBORS
#define SAD_SEARCH_VSTEP  2  // 1=FS 2=FHS
#define DECISION_THR      6 //Total number of trees is 13, decision has to be bigger than 6 to say it is a scene change.

#ifdef _DEBUG
    #define ASC_PRINTF(...)     printf(__VA_ARGS__)
    #define ASC_FPRINTF(...)    fprintf(__VA_ARGS__)
    #define ASC_FFLUSH(x)       fflush(x)
#else
    #define ASC_PRINTF(...)     __VA_ARGS__
    #define ASC_FPRINTF(...)    __VA_ARGS__
    #define ASC_FFLUSH(x)
#endif


#define SCD_CHECK_CM_ERR(STS, ERR) if ((STS) != CM_SUCCESS) { ASC_PRINTF("FAILED at file: %s, line: %d, cmerr: %d\n", __FILE__, __LINE__, STS); return ERR; }
#define SCD_CHECK_MFX_ERR(STS) if ((STS) != MFX_ERR_NONE) { ASC_PRINTF("FAILED at file: %s, line: %d, mfxerr: %d\n", __FILE__, __LINE__, STS); return STS; }

#if defined(_WIN32) || defined(_WIN64)
#define ASC_ALIGN_DECL(X) __declspec(align(X))
#else // linux
#define ASC_ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif //defined(_WIN32) || defined(_WIN64)

#endif //__ASC_DEFS__