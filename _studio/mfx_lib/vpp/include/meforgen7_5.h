//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2013 Intel Corporation. All Rights Reserved.
//

#ifndef ME4G75_H
#define ME4G75_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
//#include <memory.h>
#include <string.h>

#include "videovme.h"
//#include "VideoDefs.h"

#if defined(_WIN32) || defined(_WIN64)
  #define VME_ALIGN_DECL(X) __declspec(align(X))
#else
  #define VME_ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif

// Gen7 specific defines
#define        ADAPTIVE_CTRL        0x20
#define     B_PRUNING_ENABLE    0x40
#define        FME_REPARTITION        0x80

#define        ST_FIELD_16X8        0x04
#define        ST_FIELD_8X8        0x08

#define        VF_EXTRA_CANDIDATE    0x08
#define        VF_EARLY_SUCCESS    0x10
#define        VF_EARLY_IME_BAD    0x50

#define     OCCURRED_EARLY_SKIP        0x0080
#define     OCCURRED_EARLY_FME        0x0800
#define     OCCURRED_TOO_GOOD        0x0200
#define     OCCURRED_TOO_BAD        0x0400

#define     IMPROVED_FME             0x1000
#define     IMPROVED_BME             0x2000

#define     ALT_CAND_WON            0x4000

#define     PERFORM_SKIP            0x0001
#define     PERFORM_IME                0x0002
#define     PERFORM_1ST_FME            0x0004
#define     PERFORM_2ND_FME            0x0008
#define     PERFORM_1ST_BME            0x0010
#define     PERFORM_2ND_BME            0x0020
#define     PERFORM_INTRA            0x0040
// end

#define        DISTBIT4X4        11
#define        DISTBIT4X8        12
#define        DISTBIT8X8        13
#define        DISTBIT8X16        14
#define        DISTBIT16X16    15

#define        DISTMAX4X4        ((1<<DISTBIT4X4)-1)
#define        DISTMAX4X8        ((1<<DISTBIT4X8)-1)
#define        DISTMAX8X8        ((1<<DISTBIT8X8)-1)
#define        DISTMAX8X16        ((1<<DISTBIT8X16)-1)
#define        DISTMAX16X4        ((1<<DISTBIT16X16)-1)

#define        MAXLUTXY        0x3FF
#define        MAXLUTMODE0        0x3FF
#define        MAXLUTMODE1        0xFFF
#define        MAXEARLYTHRESH    0x3FFF

#define        INTRAMASK4X4        0x00000001
#define        INTRAMASK8X8        0x00001000
#define        INTRAMASK16X16        0x01000000
#define        INTRAMASK8X8UV        0x10000000

/*********************************************************************************\
Internal Constants
\*********************************************************************************/
#define        BHXH            1
#define        BHX8            2
#define        B8XH            4
#define        B8X8            6
#define        FHX8            10
#define        F8X8            12
#define        B4X4            10
#define        B8X4            26
#define        B4X8            34

#define        IHXH            32
#define        I4X4            33
#define        I8X8            34
#define        C4X4            35
#define        C8X8            36

#define        VER                0
#define        HOR                1
#define        DCP                2
#define        DDL                3
#define        DDR                4
#define        VRP                5
#define        HDP                6
#define        VLP                7
#define        HUP                8

#define        NO_AVAIL_A        1    // left
#define        NO_AVAIL_B        4    // top
#define        NO_AVAIL_C        2    // top-right
#define        NO_AVAIL_D        8    // top-left

#define        REFCACHESIZE    0x1000
#define     REFWINDOWWIDTH     64
#define        SKIPWIDTH        24
#define     REFWINDOWHEIGHT 32
#define     SEARCHPATHSIZE     56
#define     MAXSEARCHPATHS     64

#define        MBINVALID_DIST  0x7FFFF
#define        MBMAXDIST        0xFFFF        // for SAD
#define        SKIP16X16        0
#define        SKIP4X4            1
#define        SKIP8X8            2
// #define        MBMAXDIST        0x02badbad    // for MSE    

#define SIC_MSG 0
#define IME_MSG 1
#define FBR_MSG 2

/*********************************************************************************\
For Performance
\*********************************************************************************/
#define NUM_DEFINED_SCENARIOS        96
#define IME_COMPUTE_CLKS_PER_SU        16
#define IME_CLKS_UNI_PARTITIONING    15 //NOT VERIFIED WITH RTL
#define IME_CLKS_BI_PARTITIONING    30 //NOT VERIFIED WITH RTL


#if !defined(WIN64) && (defined (WIN32) || defined (_WIN32) || defined (WIN32E) || defined (_WIN32E) || defined(__i386__) || defined(__x86_64__))
#if defined(__INTEL_COMPILER) || (_MSC_VER >= 1300) || (defined(__GNUC__) && defined(__SSE2__) && (__GNUC__ > 3))

#define __HAAR_SAD_OPT 1
#define __SAD_OPT 1

//#if defined __SSE3__
//#define __SSSE3_ENABLE
//#endif

#endif
#else

#define __HAAR_SAD_OPT 0
#define __SAD_OPT 0

#endif

struct CREFlags
{
    U8 messageType;  //common
    U8 sourceType;  //common

    //SIC
    U8 intraChromaEn;
    U8 mode16x16En;
    U8 skipEn;
    U8 intraEn;
    U8 ftqEn;
    U8 numBi8x8SC;
    U8 skipType;
    U8 chromaInter; //common

    //FBR
    U8 hpelEn;
    U8 qpelEn;
    U8 bmeEn;
    U8 numMinors;

};

typedef struct CREFlags CreFlags;

const CreFlags CREScenarios[NUM_DEFINED_SCENARIOS] =
{
    {SIC_MSG,    0,    0,    0,    0,    1,    0,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    1,    0,    1,    0,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    1,    0,    0,    1,    0,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    1,    1,    0,    1,    0,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    4,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    4,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    0,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    4,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    0,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    4,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    0,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    3,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    0,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    3,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    0,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    2,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    0,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    2,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    0,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    0,    1,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    0,    1,    0,    0,    0,    0,    0},
    {SIC_MSG,    0,    0,    0,    1,    0,    1,    1,    1,    0,    0,    0,    0,    0},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    0},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    0},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    0},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    1,    0},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    1},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    1},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    1},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    1,    1},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    2},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    2},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    2},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    1,    2},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    3},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    3},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    3},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    1,    3},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    4},
    {FBR_MSG,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    4},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    0},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    0},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    0},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    1,    0},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    1},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    1},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    1},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    1,    1},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    2},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    2},
    {FBR_MSG,    3,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    0},
    {FBR_MSG,    3,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    0},
    {FBR_MSG,    3,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    0},
    {FBR_MSG,    3,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    1,    0},
    {FBR_MSG,    3,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    1},
    {FBR_MSG,    3,    0,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    1},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    0,    0},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    0,    0},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    1,    0},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    1,    0},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    0,    1},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    0,    1},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    1,    1},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    1,    1},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    0,    0,    2},
    {FBR_MSG,    1,    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,    0,    2},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    4,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    4,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    3,    0,    0,    1,    0,    0,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    3,    0,    0,    1,    0,    0,    4,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    3,    0,    0,    1,    0,    1,    0,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    3,    0,    0,    1,    0,    1,    4,    0,    0,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    0,    0,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    4,    0,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    0,    0,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    4,    0,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    0,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    4,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    0,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    4,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    0,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    3,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    0,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    3,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    0,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    2,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    0,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    2,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    0,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    0,    1,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    0,    1,    1,    0,    0,    0,    0},
    {SIC_MSG,    1,    0,    0,    1,    0,    1,    1,    1,    1,    0,    0,    0,    0}
};

//CRE Backend latency lookup table from RTL uArch benchmarks
const U8 computeLatencyValue[NUM_DEFINED_SCENARIOS] =
{
    72,
    82,
    84,
    92,
    38,
    54,
    90,
    106,
    38,
    54,
    90,
    106,
    38,
    51,
    90,
    102,
    38,
    48,
    90,
    98,
    38,
    47,
    90,
    94,
    44,
    72,
    136,
    104,
    44,
    72,
    120,
    92,
    44,
    72,
    104,
    80,
    44,
    72,
    88,
    68,
    44,
    72,
    36,
    56,
    100,
    72,
    36,
    56,
    84,
    60,
    36,
    56,
    32,
    48,
    76,
    56,
    32,
    48,
    36,
    56,
    100,
    72,
    36,
    56,
    84,
    60,
    36,
    56,
    30,
    38,
    58,
    66,
    26,
    30,
    42,
    46,
    30,
    38,
    58,
    66,
    30,
    38,
    58,
    66,
    30,
    37,
    58,
    64,
    30,
    36,
    58,
    62,
    30,
    33,
    58,
    60    
};

/*********************************************************************************/
class MEforGen75
    /*********************************************************************************/
{
public:
    MEforGen75( );
    ~MEforGen75( );

    // Main VME call - Running ME Engine with outputs:
    Status  LoadIntraSkipMask( );
    Status    RunSIC( mfxVME7_5UNIOutput*);
    Status    RunSIC( mfxVME7_5UNIIn*, mfxVME7_5SICIn*, mfxVME7_5UNIOutput*);
    Status    RunSICChromaPre();
    Status    RunIME( mfxVME7_5UNIOutput*, mfxVME7_5IMEOutput*);
    Status    RunIMEChromaPre();
    Status    RunIME( mfxVME7_5UNIIn*, mfxVME7_5IMEIn*, mfxVME7_5UNIOutput*, mfxVME7_5IMEOutput*);
    Status    RunIMEChromaPost();
    Status    RunFBR( mfxVME7_5UNIOutput*);
    Status    RunFBR( mfxVME7_5UNIIn*, mfxVME7_5FBRIn*, mfxVME7_5UNIOutput*);
    Status    RunFBRChromaPre();
    Status    CheckVMEInput(int message_type);

    I32        LoadFlag;

private:

    // state flags:
    I32        IntraSkipMask;
    I32        dummy;
    mfxVME7_5UNIIn    Vsta;
    mfxVME7_5SICIn    VSICsta;
    mfxVME7_5IMEIn    VIMEsta;
    mfxVME7_5FBRIn    VFBRsta;
    mfxVME7_5UNIOutput    *Vout;
    mfxVME7_5IMEOutput    *VIMEout;
    mfxI16PAIR    SkipCenterOrigChr[4][2];

    // C-model storage:
    U8      SrcMB[256],SrcFieldMB[256],SrcUV[128],SrcMBU[256],SrcMBV[256];//,SrcUV_NV12[128];
    U8      RefCache[REFCACHESIZE], *RefPix[2];
    U8      RefCacheCb[REFCACHESIZE], *RefPixCb[2];
    U8      RefCacheCr[REFCACHESIZE], *RefPixCr[2];
    U8        SkipRef[2][SKIPWIDTH*SKIPWIDTH];
    U8      SearchPath[SEARCHPATHSIZE];
    U16        Records[32], *Record[2];
    I16      LutXY[66], LutMode[12];
    U8        Intra8x8PredMode[4];
    U8        Intra4x4PredMode[16];
    U8        ChromaPredMode;
    U8PAIR    ActiveSP[2][MAXSEARCHPATHS];
    U8      Dynamic[2], SadMethod;
    I16     LenSP[2],CntSU[2];
    I32        Dist[2][42]; // Best distortion for individual blocks
    I32        DistForReducedMB[2][42]; // Best distortion for individual blocks
    I32        MajorDist[5],DistInter,Pre_FME_DistInter,Pre_BME_DistInter,MajorDistFreeMV[4],UniOpportunitySad[2][4];
    bool    Pick8x8Minor;
    I16PAIR    Best[2][42]; // Best motion vector for individual MVs
    U8        BestRef[2][10]; // Best ref id for individual major MVs
    U8        RefID[2][4];
    U8        MultiReplaced[2][10];
    I16PAIR    BestForReducedMB[2][42]; // Best motion vector for individual MVs
    U16        MajorMode[5];  // where keep the best four candidates.
    U16        UniDirMajorMode[2][5]; // major mode for each direction
    I32        UniDirMajorDist[2][5]; // major distortion for each direction
    I32        NextRef, MoreSU[2], FoundBi, SkipMask, SkipDistMb, AltMoreSU, OutSkipDist, BestIntraDist, BestIntraChromaDist, DistI16, DistIntra4or8;
    bool    CheckNextReqValid;
    U8PAIR    NextSU;
    U16        EarlySkipSuc, EarlyFmeExit, EarlyImeStop, ImeTooGood, ImeTooBad, ErrTolerance, PruningThr; // JMHTODO: Delete most of these when done.
    I32        SkipDist[4], SkipDistAddMV8x8[4], SkipDistAddMV16x16;
    bool    L0_BWD_Penalty;
    bool    bDirectReplaced;
    U8        MainCnts[2][7];
    U8        AltCnts[2][7];
    U8        BmeCnt[2][4];
    U8        NumSUinIME_Adaptive;
    U32        SICTime;
    U32        IMETime;
    U32        FBRTime;
    U8        ShapeValidMVCheck[4], OrigShapeMask;
    bool    PipelinedSUFlg;
    bool    UsePreviousMVFlg;
    U8PAIR    SavedSU;
    U8        SavedRefId;
    I16PAIR    PrevMV[2][4];
    bool    bdualAdaptive;
    I16        AdaptiveCntSu[2], FixedCntSu[2];
    U8        bSkipSbCheckMode;
    int        SubBlockMaxSad; // HSW overloading: 16x16 dist OR max 4x4 dist OR max 8x8 dist
    bool    bSkipSbFailed;
    //bool    bPruneBi[4];
    bool    IsField, IsBottom[2];
    I16PAIR    nullMV;
    bool    startedAdaptive;
    U8        UniSkips,BiSkips;
    U8        PipelineCalls;
    I32        m_BBSChromaDistortion[2][4];
    I32        MBMaxDist;
    // Intra functions in INTRA:    
    int        Intra16x16SearchUnit( );
    int        Intra8x8SearchUnit( );
    int        Intra8x8ChromaSearchUnit( );
    int        Intra4x4SearchUnit( );
    U32        GetSadChroma(U8 *in_Src, U8 *in_Blk, U32 in_N);
    int        IntraPredict8x8(U8 *src, U8 *lft, U8 *top, int edge, U8 pmode, U8 *mode, I32 *dist);
    int        IntraPredict4x4(U8 *src, U8 *lft, U8 *top, int edge, U8 pmode, U8 *mode, I32 *dist);
    int        IntraAVS8x8SearchUnit( );
    int        IntraPredictAVS8x8(U8 *src, U8 *lft,  U8 *lftbottom, U8 *top, U8 *topright, int edge, U8 *mode, I32 *dist);

    // Inter integer search functions in IME:    
    int        IntegerMESearchUnit( );
    int        OneSearchUnit(U8 qx, U8 qy, short isb);
    int        GetNextSU( );
    int        SetupSearchPath( );
    int        SetupLutCosts( );
    void    SelectBestMinorShape(I32 *MinorDist, U16 *MinorMode, U8 IgnoreMaxMv );
    void    GetControlledDynamicSetting(int recordid);
    bool    CombineReducuedMBResults(short isb);
    void    GetDynamicSU(int bDualRecord);
    void    GetPipelinedDynamicSetting(int recordid);
    void    PickSavedSU(int bDualRecord);
    void    Select2SUs(int bDualRecord, bool in_bCheckInFlightSU);
    U8        GetValid8x8MVs(I16PAIR *out_ValidMVs, bool bDualRecord);
    bool    CheckDynamicEnable(I16 in_SuCntRef0, I16 in_SuCntRef1);
    void    CheckHorizontalAdaptiveSU(I16PAIR ValidMV, 
        bool bCheckInFlightSU, 
        bool *NbSPWalkState,
        U8 refNum,
        bool &out_bAtLeastOneSU);

    void    CheckVerticalAdaptiveSU(I16PAIR ValidMV, 
        bool bCheckInFlightSU, 
        bool *NbSPWalkState,
        U8 refNum,
        bool &out_bAtLeastOneSU);

    void    CheckDiagonalAdaptiveSU(I16PAIR ValidMV, 
        bool bCheckInFlightSU, 
        bool *NbSPWalkState,
        U8 refNum,
        bool &out_bAtLeastOneSU);

    // Inter refinement search functions in FME:    
    void    FractionalMESearchUnit(U8 mode, U8 submbshape, U8 subpredmode, U8 precision, bool chroma);
    int        SubpelRefineBlock(U8 *qsrc, int dist_idx, int bw, int bh, int dx, int dy, int isb, U8 precision, bool chroma, int refid);
    int        SubpelRefineBlockField(U8 *qsrc, I16PAIR *mvs, I32 *dist, int bw, int bh, int dx, int dy, int isb, bool half_pel);
    bool    CheckBidirValid();//int mode, bool checkPruning, bool checkGlobalReplacement);
    //void    CheckPruning(int mode);

    //Partitioning related functions
    //void    MajorModeDecision(U8& disableAltMask, U8& AltNumMVsRequired, bool& disableAltCandidate);
    void    MajorModeDecision( );
    void    PartitionBeforeModeDecision( );
    void    PreBMEPartition(I32 *dist, U16 *mode, bool benablecopy);

    bool    SelectBestAndAltShape(U8 RefIndex, U8& BestMode, U8& AltMode);

    // Inter bidirectional search functions in BME:    
    int        BidirectionalMESearchUnit(I32 *dist, U8 *SubPredMode);
    int        BidirectionalMESearchUnitChroma(I32 *dist, U8 *SubPredMode);
    int        CheckSkipUnit( int in8x8 );
    //void    Direct8x8Replacement(  );
    void    CheckUnidirectional(U8 *src_mb, U8 *ref, I16PAIR mv, I32 refidx, I32 bw, I32 bh, I32 *d0, I32 *d1, U8 blkidx, bool chromaU, bool chromaV);
    int        CheckBidirectionalField(U8 *src, I16PAIR mv0, I16PAIR mv1, I32 bw, I32 bh, I32 *d0, I32 *d1);
    void    CheckBidirectional(U8 *src, U8 *ref0, U8 *ref1, I16PAIR mv0, I16PAIR mv1, I32 bw, I32 bh, I32 mvx_offset, I32 mvy_offset, I32 *d0, I32 *d1, int refid0, int refid1);
    void    CheckSkipBidirectional(U8 *src, U8 *ref0, U8 *ref1, I16PAIR mv0, I16PAIR mv1, I32 bw, I32 bh, I32 mvx_offset, I32 mvy_offset, I32 *d0, I32 *d1, U8 blkidx, bool chromaU, bool chromaV);
    int        AvsMirrorBestMajorMVs( );

    // funcitons in UTIL
    void    LoadSrcMB( );            
    void    PadReference(U8 *Ref);
    void    LoadReference(U8 *src, U8 *dest, U8 dest_w, U8 dest_h, I16PAIR mvs, U8 width, bool isBottom);
#if (__HAAR_SAD_OPT == 1)
    void    LoadReference2(U8 *src, U8 *dest, U8 dest_w, U8 dest_h, I16PAIR mvs, U8 width, bool isBottom,
        bool &copied, U8* &src_shifted, int &src_pitch);
#endif
    int        GetCostXY(int x, int y, int isb);
    int        GetCostRefID(int refid);
    int        GetSadBlock(U8 *src, U8 *ref, int bw, int bh, bool bChroma=false, bool VSelect=false);
    int        GetFtqBlock(U8 *src, U8 *ref, int bw, int bh, U8 blkidx);
    int        GetFtqBlockUV(U8 *src, U8 *ref, int bw, int bh, U8 blkidx);
    int        GetSadBlockField(U8 *src, U8 *ref, int bw, int bh);
    int        GetFractionalPixelRow(U8 *bb, U8 *ref, short qx, short bw, short *mx);
#if (__HAAR_SAD_OPT == 1)
    int        GetReferenceBlock(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh,  int src_pitch=0);
#else
    int        GetReferenceBlock(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh);
#endif
    int        CheckAndGetReferenceBlock(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh);
#if (__HAAR_SAD_OPT == 1)
    int        GetReferenceBlock2Tap(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh, int src_pitch=0);
    int        GetReferenceBlock4Tap(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh, int src_pitch=0);
#else
    int        GetReferenceBlock2Tap(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh);
    int        GetReferenceBlock4Tap(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh);
#endif
#if (__HAAR_SAD_OPT == 1)
    inline void VertInterp4Tap (U8 *input0, U8 *output, int pitch, int bh, int bw, int pitch_out);
    inline void HorInterp4Tap (U8 *input0, U8 *output, int pitch_in, int bh, int bw, int pitch_out);
    inline void FinalInterp4Tap (U8 *input0, U8 *input1, U8 *output, int bh, int bw);
#endif
    int        GetReferenceBlock6Tap(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh);
    int        GetReferenceBlock6TapS(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh);
    int        GetReferenceBlockField(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh);
    //void    GetDistortionSU(U8 *ref, I32 *dif);
    void    GetDistortionSU(U8 *src, U8 *ref, I32 *dif);
    void    GetDistortionSUChroma(U8 *srcU, U8 *srcV, U8 *refU, U8 *refV, I32 *dif);
    int        GetDistortionSUField(U8 *ref, I32 *dif);
    int        SummarizeFinalOutputIME( );
    int        SummarizeFinalOutputSIC( );
    int        SummarizeFinalOutputFBR( );
    void    AssignMV(int mode, int k, int shape, int bi=2); // bi = 2 for IME and bi is set for fbr by subpredmode, Enables output of winning MV for FBR
    void    UpdateBest( );
    void    UpdateRecordOut( );
    bool    CheckTooGoodOrBad();
    void    Replicate8MVs(U8 interMbMode);
    void    GetCREComputeClkCount(U8 message_type);
    void    GetIMEComputeClkCount();
    U8        FindScenarioIdx(CreFlags& in_CREScenarioFlags);

#if (__HAAR_SAD_OPT == 1)
    void    GetSad4x4_par(U8 *src, U8 *ref, int rw, int *sad);
    void    GetFourSAD4x4(U8* pMB, int SrcStep, U8* pSrc, U32* dif);
#endif
    int        GetSad4x4(U8 *src, U8 *ref, int rw);
    U32        GetFtq4x4(U8 *src, U8 *ref, int rw);
    int        GetSad4x4F(U8 *src, U8 *ref, int rw); // for field case
    int        GetSad8x8(U8 *src, U8 *ref);    // sw = 16; rw = 8;
    int        GetSad16x16(U8 *src, U8 *ref);    // sw = 16; rw = 16;
#if (__HAAR_SAD_OPT == 1)
    int        GetSad16x16_dist(U8 *src, U8 *ref, int dist);    // sw = 16; rw = 16;
#endif

    bool    IsMinorShape(U8 dist_idx) const;
    // For reduced mb cases
    void    ReplicateSadMV();
    void    ReplicateSadMVStreamIn(); //IME only
    void    ScaleDist(I32& Dist);
    bool    SetupSkipMV();
    void    CleanRecordOut();

};

void GetFourSAD4x4_viaIPP(U8* pMB, int SrcStep, U8* pSrc, U32* dif);

#endif