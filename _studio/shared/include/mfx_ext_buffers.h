/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_EXT_BUFFERS_H__
#define __MFX_EXT_BUFFERS_H__

#include "vm_strings.h"

// internal(undocumented) handles for VideoCORE::SetHandle
#define MFX_HANDLE_TIMING_LOG       ((mfxHandleType)1001)
#define MFX_HANDLE_EXT_OPTIONS      ((mfxHandleType)1002)
#define MFX_HANDLE_TIMING_SUMMARY   ((mfxHandleType)1003)
#define MFX_HANDLE_TIMING_TAL       ((mfxHandleType)1004)

// enum for MFX_HANDLE_EXT_OPTIONS
enum eMFXExtOptions
{
    MFX_EXTOPTION_DEC_CUSTOM_GUID   = 0x0001,
    MFX_EXTOPTION_DEC_STANDARD_GUID = 0x0002,
    MFX_EXTOPTION_VPP_SW            = 0x0004,
    MFX_EXTOPTION_VPP_BLT           = 0x0008,
    MFX_EXTOPTION_VPP_FASTCOMP      = 0x0010,
};

#define MFX_EXTBUFF_DDI MFX_MAKEFOURCC('D','D','I','P')

typedef struct {
    mfxExtBuffer Header;

    // parameters below exist in DDI but doesn't exist in MediaSDK public API
    mfxU16  IntraPredCostType;      // from DDI: 1=SAD, 2=SSD, 4=SATD_HADAMARD, 8=SATD_HARR
    mfxU16  MEInterpolationMethod;  // from DDI: 1=VME4TAP, 2=BILINEAR, 4=WMV4TAP, 8=AVC6TAP
    mfxU16  MEFractionalSearchType; // from DDI: 1=FULL, 2=HALF, 4=SQUARE, 8=HQ, 16=DIAMOND
    mfxU16  MaxMVs;
    mfxU16  SkipCheck;              // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16  DirectCheck;            // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16  BiDirSearch;            // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16  MBAFF;                  // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16  FieldPrediction;        // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16  RefOppositeField;       // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16  ChromaInME;             // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16  WeightedPrediction;     // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16  MVPrediction;           // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON

    // parameters below exist in both DDI and MediaSDK but interpetation differs
    struct {
        mfxU16 IntraPredBlockSize;  // from DDI, mask of 1=4x4, 2=8x8, 4=16x16, 8=PCM
        mfxU16 InterPredBlockSize;  // from DDI, mask of 1=16x16, 2=16x8, 4=8x16, 8=8x8, 16=8x4, 32=4x8, 64=4x4
    } DDI;

    // MediaSDK parametrization
    mfxU16  BRCPrecision;   // 0=default=normal, 1=lowest, 2=normal, 3=highest
    mfxU16  RefRaw;         // (tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON) on=vme reference on raw(input) frames, off=reconstructed frames
    mfxU16  reserved0;
    mfxU16  ConstQP;        // disable bit-rate control and use constant QP
    mfxU16  GlobalSearch;   // 0=default, 1=long, 2=medium, 3=short
    mfxU16  LocalSearch;    // 0=default, 1=type, 2=small, 3=square, 4=diamond, 5=large diamond, 6=exhaustive, 7=heavy horizontal, 8=heavy vertical

    mfxU16 EarlySkip;       // 0=default (let driver choose), 1=enabled, 2=disabled
    mfxU16 LaScaleFactor;   // 0=default (let msdk choose), 1=1x, 2=2x, 4=4x
    mfxU16 reserved1;       //
    mfxU16 reserved2;       //
    mfxU16 StrengthN;       // strength=StrengthN/100.0
    mfxU16 FractionalQP;    // 0=disabled (default), 1=enabled

    mfxU16 NumActiveRefP;   //
    mfxU16 NumActiveRefBL0; //

    mfxU16 DisablePSubMBPartition;  // tri-state, default depends on Profile and Level
    mfxU16 DisableBSubMBPartition;  // tri-state, default depends on Profile and Level
    mfxU16 WeightedBiPredIdc;       // 0 - off, 1 - explicit (unsupported), 2 - implicit
    mfxU16 DirectSpatialMvPredFlag; // (tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON)on=spatial on, off=temporal on
    mfxU16 reserved3;       // 0..31
    mfxU16 reserved4;       // 0..255
    mfxU16 BiPyramid;               // 0 - no ref b, 1 - one ref b, 2 - pyramid
    mfxU16 CabacInitIdcPlus1;       // 0 - use default value, 1 - cabac_init_idc = 0 and so on
    mfxU16 NumActiveRefBL1;         //
    mfxU16 QpUpdateRange;           // 
    mfxU16 RegressionWindow;        //
    mfxU16 LookAheadDependency;     // LookAheadDependency < LookAhead
    mfxU16 Hme;                     // tri-state

} mfxExtCodingOptionDDI;


#define MFX_EXTBUFF_QM MFX_MAKEFOURCC('E','X','Q','P')

typedef struct {
    mfxExtBuffer Header;
    mfxU16 bIntraQM;    
    mfxU16 bInterQM;
    mfxU16 bChromaIntraQM;    
    mfxU16 bChromaInterQM;
    mfxU8 IntraQM[64];
    mfxU8 InterQM[64];
    mfxU8 ChromaIntraQM[64];
    mfxU8 ChromaInterQM[64];
} mfxExtCodingOptionQuantMatrix;

enum
{
    MFX_MB_NOP = 0,
    MFX_MB_WRITE_TEXT,
    MFX_MB_WRITE_BIN,
    MFX_MB_READ_BIN
};

#define MFX_EXTBUFF_DUMP MFX_MAKEFOURCC('D','U','M','P')
enum
{
    MFX_MAX_PATH                = 260
};

typedef struct {
    mfxExtBuffer Header;

    vm_char MBFilename[MFX_MAX_PATH];
    mfxU32  MBFileOperation;

    vm_char ReconFilename[MFX_MAX_PATH];
    vm_char InputFramesFilename[MFX_MAX_PATH];

} mfxExtDumpFiles;


// aya: should be moved on MSDK API level after discussion
#define MFX_EXTBUFF_VPP_VARIANCE_REPORT MFX_MAKEFOURCC('V','R','P','F')

#pragma warning (disable: 4201 ) /* disable nameless struct/union */
typedef struct {
    mfxExtBuffer    Header;
    union{
        struct{
            mfxU32  SpatialComplexity;
            mfxU32  TemporalComplexity;
       };
       struct{
           mfxU16  PicStruct;
           mfxU16  reserved[3];
       };
   };

    mfxU16          SceneChangeRate;
    mfxU16          RepeatedFrame;

    // variances 
    mfxU32          Variances[11];
} mfxExtVppReport;

#define MFX_EXTBUFF_HEVCENC MFX_MAKEFOURCC('B','2','6','5')
typedef struct {
    mfxExtBuffer Header;

    mfxU16      Log2MaxCUSize;
    mfxU16      MaxCUDepth;
    mfxU16      QuadtreeTULog2MaxSize;
    mfxU16      QuadtreeTULog2MinSize;
    mfxU16      QuadtreeTUMaxDepthIntra;
    mfxU16      QuadtreeTUMaxDepthInter;
    mfxU16      AnalyzeChroma;              /* tri-state option */
    mfxU16      SignBitHiding;
    mfxU16      RDOQuant;
    mfxU16      SAO;
    mfxU16      SplitThresholdStrengthCUIntra;
    mfxU16      SplitThresholdStrengthTUIntra;
    mfxU16      SplitThresholdStrengthCUInter;
    mfxU16      IntraNumCand1_2;
    mfxU16      IntraNumCand1_3;
    mfxU16      IntraNumCand1_4;
    mfxU16      IntraNumCand1_5;
    mfxU16      IntraNumCand1_6;
    mfxU16      IntraNumCand2_2;
    mfxU16      IntraNumCand2_3;
    mfxU16      IntraNumCand2_4;
    mfxU16      IntraNumCand2_5;
    mfxU16      IntraNumCand2_6;
    mfxU16      WPP;
    mfxU16      GPB;
    mfxU16      AMP;
} mfxExtCodingOptionHEVC;

#endif // __MFX_EXT_BUFFERS_H__
