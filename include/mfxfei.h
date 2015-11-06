/******************************************************************************* *\

Copyright (C) 2013-2015 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfxfei.h

*******************************************************************************/
#ifndef __MFXFEI_H__
#define __MFXFEI_H__
#include "mfxdefs.h"
#include "mfxvstructures.h"
#include "mfxpak.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef struct {
    mfxExtBuffer    Header;

    mfxU16    Qp;
    mfxU16    LenSP;
    mfxU16    SearchPath;
    mfxU16    SubMBPartMask;
    mfxU16    SubPelMode;
    mfxU16    InterSAD;
    mfxU16    IntraSAD;
    mfxU16    AdaptiveSearch;
    mfxU16    MVPredictor;
    mfxU16    MBQp;
    mfxU16    FTEnable;
    mfxU16    IntraPartMask;
    mfxU16    RefWidth;
    mfxU16    RefHeight;
    mfxU16    SearchWindow;
    mfxU16    DisableMVOutput;
    mfxU16    DisableStatisticsOutput;
    mfxU16    PictureType; /* Input picture type*/
    mfxU16    RefPictureType[2]; /* reference picture type, 0 -L0, 1 - L1*/
    mfxU16    Enable8x8Stat;
    mfxU16    reserved[38];
} mfxExtFeiPreEncCtrl;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32  reserved[13];
    mfxU32  NumMBAlloc; /* size of allocated memory in number of macroblocks */

    struct  mfxExtFeiPreEncMVPredictorsMB {
        mfxI16Pair MV[2]; /* 0 for L0 and 1 for L1 */
    } *MB;
} mfxExtFeiPreEncMVPredictors;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32  reserved[13];
    mfxU32  NumQPAlloc; /* size of allocated memory in number of QPs value*/

    mfxU8    *QP;
} mfxExtFeiEncQP;

//1 PreENC output
/* Layout is exactly the same as mfxExtFeiEncMVs, this buffer may be removed in future */
typedef struct {
    mfxExtBuffer    Header;
    mfxU32  reserved[13];
    mfxU32  NumMBAlloc;

    struct  mfxExtFeiPreEncMVMB {
        mfxI16Pair MV[16][2];
    } *MB;
} mfxExtFeiPreEncMV;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32 reserved0[13];
    mfxU32  NumMBAlloc;

    struct  mfxExtFeiPreEncMBStatMB {
        struct  {
            mfxU16  BestDistortion;
            mfxU16  Mode ;
        } Inter[2]; /*0 -L0, 1 - L1*/

        mfxU16  BestIntraDistortion;
        mfxU16  IntraMode ;

        mfxU16  NumOfNonZeroCoef;
        mfxU16  reserved1;

        mfxU32  SumOfCoef;

        mfxU32  reserved2;

        mfxU32  Variance16x16;

        mfxU32  Variance8x8[4];

        mfxU32  PixelAverage16x16;

        mfxU32  PixelAverage8x8[4];
    } *MB;
} mfxExtFeiPreEncMBStat;

//1  ENC_PAK input
typedef struct {
    mfxExtBuffer    Header;

    mfxU16    SearchPath;
    mfxU16    LenSP;
    mfxU16    SubMBPartMask;
    mfxU16    IntraPartMask;
    mfxU16    MultiPredL0;
    mfxU16    MultiPredL1;
    mfxU16    SubPelMode;
    mfxU16    InterSAD;
    mfxU16    IntraSAD;
    mfxU16    DistortionType;
    mfxU16    RepartitionCheckEnable;
    mfxU16    AdaptiveSearch;
    mfxU16    MVPredictor;
    mfxU16    NumMVPredictors;
    mfxU16    PerMBQp;
    mfxU16    PerMBInput;
    mfxU16    MBSizeCtrl;
    mfxU16    RefWidth;
    mfxU16    RefHeight;
    mfxU16    SearchWindow;
    mfxU16    ColocatedMbDistortion;
    mfxU16    reserved[39];
} mfxExtFeiEncFrameCtrl;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32  reserved[13];
    mfxU32  NumMBAlloc; /* size of allocated memory in number of macroblocks */

    struct  mfxExtFeiEncMVPredictorsMB {
        struct {
            mfxU8   RefL0: 4;
            mfxU8   RefL1: 4;
        } RefIdx[4]; /* index is predictor number */
        mfxU32      reserved;
        mfxI16Pair MV[4][2]; /* first index is predictor number, second is 0 for L0 and 1 for L1 */
    } *MB;
} mfxExtFeiEncMVPredictors;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32  reserved[13];
    mfxU32  NumMBAlloc;

    struct  mfxExtFeiEncMBCtrlMB {
        mfxU32    ForceToIntra     : 1;
        mfxU32    ForceToSkip      : 1;
        mfxU32    ForceToNoneSkip  : 1;
        mfxU32    reserved1        : 29;

        mfxU32    reserved2;
        mfxU32    reserved3;

        mfxU32    reserved4        : 16;
        mfxU32    TargetSizeInWord : 8;
        mfxU32    MaxSizeInWord    : 8;
    } *MB;
} mfxExtFeiEncMBCtrl;


//1 ENC_PAK output
/* Buffer holds 32 MVs per MB. MVs are located in zigzag scan order.
Number in diagram below shows location of MV in memory.
For example, MV for right top 4x4 sub block is stored in 5-th element of the array.
========================
|| 00 | 01 || 04 | 05 ||
------------------------
|| 02 | 03 || 06 | 07 ||
========================
|| 08 | 09 || 12 | 13 ||
------------------------
|| 10 | 11 || 14 | 15 ||
========================
*/
typedef struct {
    mfxExtBuffer    Header;
    mfxU32  reserved[13];
    mfxU32  NumMBAlloc;

    struct  mfxExtFeiEncMVMB {
        mfxI16Pair MV[16][2]; /* first index is block (4x4 pixels) number, second is 0 for L0 and 1 for L1 */
    } *MB;
} mfxExtFeiEncMV;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32  reserved[13];
    mfxU32  NumMBAlloc;

    struct mfxExtFeiEncMBStatMB {
        mfxU16  InterDistortion[16];
        mfxU16  BestInterDistortion;
        mfxU16  BestIntraDistortion;
        mfxU16  ColocatedMbDistortion;
        mfxU16  reserved;
        mfxU32  reserved1[2];
    } *MB;
} mfxExtFeiEncMBStat;

typedef struct {
    mfxU32    reserved0[3];

    //dword 3
    mfxU32    InterMbMode         : 2;
    mfxU32    MBSkipFlag          : 1;
    mfxU32    Reserved00          : 1;
    mfxU32    IntraMbMode         : 2;
    mfxU32    Reserved01          : 1;
    mfxU32    FieldMbPolarityFlag : 1;
    mfxU32    MbType              : 5;
    mfxU32    IntraMbFlag         : 1;
    mfxU32    FieldMbFlag         : 1;
    mfxU32    Transform8x8Flag    : 1;
    mfxU32    Reserved02          : 1;
    mfxU32    DcBlockCodedCrFlag  : 1;
    mfxU32    DcBlockCodedCbFlag  : 1;
    mfxU32    DcBlockCodedYFlag   : 1;
    mfxU32    Reserved03          :12;

    //dword 4
    mfxU8     HorzOrigin;
    mfxU8     VertOrigin;
    mfxU16    CbpY;

    //dword 5
    mfxU16    CbpCb;
    mfxU16    CbpCr;

    //dword 6
    mfxU32    QpPrimeY               : 8;
    mfxU32    Reserved30             :17;
    mfxU32    MbSkipConvDisable      : 1;
    mfxU32    IsLastMB               : 1;
    mfxU32    EnableCoefficientClamp : 1;
    mfxU32    Direct8x8Pattern       : 4;

    union {
        struct {// Intra MBs
            //dword 7,8
            mfxU16   LumaIntraPredModes[4];

            //dword 9
            mfxU32   ChromaIntraPredMode : 2;
            mfxU32   IntraPredAvailFlags : 6;
            mfxU32   Reserved60          : 24;
        } IntraMB;
        struct {// Inter MBs
            //dword 7
            mfxU8    SubMbShapes;
            mfxU8    SubMbPredModes;
            mfxU16   Reserved40;

            //dword 8, 9
            mfxU8    RefIdx[2][4]; /* first index is 0 for L0 and 1 for L1 */
        } InterMB;
    };

    //dword 10
    mfxU16    Reserved70;
    mfxU8     TargetSizeInWord;
    mfxU8     MaxSizeInWord;

    mfxU32     reserved2[5];
}mfxFeiPakMBCtrl;

typedef struct {
    mfxExtBuffer    Header;
    mfxU32  reserved[13];
    mfxU32  NumMBAlloc;

    mfxFeiPakMBCtrl *MB;
} mfxExtFeiPakMBCtrl;


//1 SPS, PPS, slice header
typedef struct {
    mfxExtBuffer    Header;

    mfxU16    Pack;

    mfxU16    SPSId;
    mfxU16    Profile;
    mfxU16    Level;

    mfxU16    NumRefFrame;
    mfxU16    WidthInMBs;
    mfxU16    HeightInMBs;

    mfxU16    ChromaFormatIdc;
    mfxU16    FrameMBsOnlyFlag;
    mfxU16    MBAdaptiveFrameFieldFlag;
    mfxU16    Direct8x8InferenceFlag;
    mfxU16    Log2MaxFrameNum;
    mfxU16    PicOrderCntType;
    mfxU16    Log2MaxPicOrderCntLsb;
    mfxU16    DeltaPicOrderAlwaysZeroFlag;

//    mfxU32  reserved[];
} mfxExtFeiSPS;

typedef struct {
    mfxExtBuffer    Header;

    mfxU16    Pack;

    mfxU16    SPSId;
    mfxU16    PPSId;

    mfxU16    FrameNum; 

    mfxU16    PicInitQP;
    mfxU16    NumRefIdxL0Active;
    mfxU16    NumRefIdxL1Active;

    mfxU16    ChromaQPIndexOffset;
    mfxU16    SecondChromaQPIndexOffset;

    mfxU16    IDRPicFlag;
    mfxU16    ReferencePicFlag;
    mfxU16    EntropyCodingModeFlag;
    mfxU16    ConstrainedIntraPredFlag;
    mfxU16    Transform8x8ModeFlag;

//    mfxU32  reserved[];
} mfxExtFeiPPS;

typedef struct {
    mfxExtBuffer    Header;
//    mfxU32  reserved[];
    mfxU16    NumSliceAlloc;
    mfxU16    NumSlice;
    mfxU16    Pack;

    struct mfxSlice{
        mfxU16    MBAaddress;
        mfxU16    NumMBs;
        mfxU16    SliceType;
        mfxU16    PPSId;
        mfxU16    IdrPicId;

        mfxU16    CabacInitIdc;

        mfxU16    SliceQPDelta;
        mfxU16    DisableDeblockingFilterIdc;
        mfxI16    SliceAlphaC0OffsetDiv2;
        mfxI16    SliceBetaOffsetDiv2;
        //mfxU32  reserved[];

        struct {
            mfxU16   PictureType; 
            mfxU16   Index;
        } RefL0[32], RefL1[32]; /* index in  mfxPAKInput::L0Surface array */

    } *Slice;
}mfxExtFeiSliceHeader;


typedef struct {
    mfxExtBuffer    Header;

    mfxU16  DisableHME;  // 0 -enable, any other value means disable 
    mfxU16  DisableSuperHME;
    mfxU16  DisableUltraHME;

    mfxU16     reserved[57];
} mfxExtFeiCodingOption;


//1 functions
typedef enum {
    MFX_FEI_FUNCTION_PREENC     =1,
    MFX_FEI_FUNCTION_ENCPAK     =2,
    MFX_FEI_FUNCTION_ENC        =3,
    MFX_FEI_FUNCTION_PAK        =4
} mfxFeiFunction;

enum {
    MFX_EXTBUFF_FEI_PARAM          = MFX_MAKEFOURCC('F','E','P','R'),
    MFX_EXTBUFF_FEI_PREENC_CTRL    = MFX_MAKEFOURCC('F','P','C','T'),
    MFX_EXTBUFF_FEI_PREENC_MV_PRED = MFX_MAKEFOURCC('F','P','M','P'),
    MFX_EXTBUFF_FEI_PREENC_QP      = MFX_MAKEFOURCC('F','P','Q','P'),
    MFX_EXTBUFF_FEI_PREENC_MV      = MFX_MAKEFOURCC('F','P','M','V'),
    MFX_EXTBUFF_FEI_PREENC_MB      = MFX_MAKEFOURCC('F','P','M','B'),
    MFX_EXTBUFF_FEI_ENC_CTRL       = MFX_MAKEFOURCC('F','E','C','T'),
    MFX_EXTBUFF_FEI_ENC_MV_PRED    = MFX_MAKEFOURCC('F','E','M','P'),
    MFX_EXTBUFF_FEI_ENC_MB         = MFX_MAKEFOURCC('F','E','M','B'),
    MFX_EXTBUFF_FEI_ENC_MV         = MFX_MAKEFOURCC('F','E','M','V'),
    MFX_EXTBUFF_FEI_ENC_MB_STAT    = MFX_MAKEFOURCC('F','E','S','T'),
    MFX_EXTBUFF_FEI_PAK_CTRL       = MFX_MAKEFOURCC('F','K','C','T'),
    MFX_EXTBUFF_FEI_SPS            = MFX_MAKEFOURCC('F','S','P','S'),
    MFX_EXTBUFF_FEI_PPS            = MFX_MAKEFOURCC('F','P','P','S'),
    MFX_EXTBUFF_FEI_SLICE          = MFX_MAKEFOURCC('F','S','L','C'),
    MFX_EXTBUFF_FEI_CODING_OPTION  = MFX_MAKEFOURCC('F','C','D','O')
};

/* should be attached to mfxVideoParam during initialization to indicate FEI function */
typedef struct {
    mfxExtBuffer    Header; 
    mfxFeiFunction  Func;
    mfxU16  SingleFieldProcessing;
    mfxU16 reserved[57];
} mfxExtFeiParam;


#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */


#endif

