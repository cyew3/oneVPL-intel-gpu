/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_EXT_BUFFERS_H__
#define __MFX_EXT_BUFFERS_H__

#define ADVANCED_REF

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

#define MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK MFX_MAKEFOURCC('A','P','B','K')

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
    mfxU16 LongStartCodes;          // tri-state, use long start-codes for all NALU
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
    mfxU16      QuadtreeTUMaxDepthInterRD;
    mfxU16      AnalyzeChroma;      // tri-state, look for chroma intra mode
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
    mfxU16      reserved0;
    mfxU16      PartModes;          // 0-default; 1-square only; 2-no AMP; 3-all
    mfxU16      CmIntraThreshold;   // threshold = CmIntraThreshold / 256.0
    mfxU16      TUSplitIntra;       // 0-default 1-always 2-never 3-for Intra frames only
    mfxU16      CUSplit;            // 0-default 1-always 2-check Skip cost first
    mfxU16      IntraAngModes;      // I slice Intra Angular modes: 0-default 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99 -DC&Planar only
    mfxU16      EnableCm;           // tri-state
    mfxU16      BPyramid;           // tri-state
    mfxU16      FastPUDecision;     // tri-state
    mfxU16      HadamardMe;         // 0-default 1-never; 2-subpel; 3-always
    mfxU16      TMVP;               // tri-state
    mfxU16      Deblocking;         // tri-state
    mfxU16      RDOQuantChroma;     // tri-state
    mfxU16      RDOQuantCGZ;        // tri-state
    mfxU16      SaoOpt;             // 0-default; 1-all modes; 2-fast four modes only, 3-one first mode only
    mfxU16      SaoSubOpt;          // 0-default; 1-All; 2-SubOpt, 3-Ref Frames only
    mfxU16      IntraNumCand0_2;    // number of candidates for SATD stage after gradient analysis for TU4x4
    mfxU16      IntraNumCand0_3;    // number of candidates for SATD stage after gradient analysis for TU8x8
    mfxU16      IntraNumCand0_4;    // number of candidates for SATD stage after gradient analysis for TU16x16
    mfxU16      IntraNumCand0_5;    // number of candidates for SATD stage after gradient analysis for TU32x32
    mfxU16      IntraNumCand0_6;    // number of candidates for SATD stage after gradient analysis for TU64x64
    mfxU16      CostChroma;         // tri-state, include chroma in cost
    mfxU16      PatternIntPel;      // 0-default; 1-log; 100-fullsearch
    mfxU16      FastSkip;           // tri-state
    mfxU16      PatternSubPel;      // 0-default; 1-int pel only; 2-halfpel; 3-quarter pel
    mfxU16      ForceNumThread;     // 0-default
    mfxU16      FastCbfMode;        // tri-state, stop PU modes after cbf is 0
    mfxU16      PuDecisionSatd;     // tri-state, use SATD for PU decision
    mfxU16      MinCUDepthAdapt;    // tri-state, adaptive min CU depth
    mfxU16      MaxCUDepthAdapt;    // tri-state, adaptive max CU depth
    mfxU16      NumBiRefineIter;    // 1-check best L0+L1; N-check best L0+L1 then N-1 refinement iterations
    mfxU16      CUSplitThreshold;   // skip CU split check: threshold = CUSplitThreshold / 256.0
    mfxU16      DeltaQpMode;        // DeltaQP modes: 0-default 1-disabled; 2-paq; 3-calq; 4-paq+calq
    mfxU16      Enable10bit;        // tri-state
    mfxU16      IntraAngModesP;     // P slice Intra Angular modes: 0-default 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99 -DC&Planar only, 100- disable
    mfxU16      IntraAngModesBRef;  // B Ref slice Intra Angular modes: 0-default 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99 -DC&Planar only, 100- disable
    mfxU16      IntraAngModesBnonRef;// B non Ref slice Intra Angular modes: 0-default 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99 -DC&Planar only, 100- disable
    mfxU16      IntraChromaRDO;     // tri-state, adjusted intra chroma RDO 
    mfxU16      FastInterp;         // tri-state, adjusted intra chroma RDO 
    mfxU16      SplitThresholdTabIndex;// 0,1,2 to select table
    mfxU16      CpuFeature;         // 0-auto, 1-px, 2-sse4, 3-sse4atom, 4-ssse3, 5-avx2
    mfxU16      TryIntra;           // Try Intra in Inter: 0-default, 1-Always, 2-Based on Content Analysis
    mfxU16      FastAMPSkipME;      // 0-default, 1-never, 2-Skip AMP ME of Large Partition when Skip is best
    mfxU16      FastAMPRD;          // 0-default, 1-never, 2-Adaptive Fast Decision
    mfxU16      SkipMotionPartition;          // 0-default, 1-never, 2-Adaptive
    mfxU16      SkipCandRD;         // on-Full RD, off-fast decision
    mfxU16      FramesInParallel;   // number of frames for encoding at the same time (0 - auto detect, 1 - default, no frame threading).
    mfxU16      AdaptiveRefs;      // on / off
    mfxU16      FastCoeffCost;
    mfxU16      NumRefFrameB;       // 0-1-default, 2+ use given
    mfxU16      IntraMinDepthSC;    // 0-default, 1+ use given
    mfxU16      InterMinDepthSTC;   // 0-default, 1+ use given
    mfxU16      MotionPartitionDepth;   // 0-default, 1+ use given
    mfxU16      reserved1;          // 
    mfxU16      AnalyzeCmplx;       // 0-default, 1-off, 2-on
    mfxU16      RateControlDepth;   // how many frames analyzed by BRC including current frame
    mfxU16      LowresFactor;       // downscale factor for analyze complexity: 0-default 1-fullsize 2-halfsize 3-quartersize
    mfxU16      DeblockBorders;     // tri-state, deblock borders
    mfxU16      SAOChroma;          // on / off
    mfxU16      RepackProb;         // percent of random repack probabiility, 0 - no random repacks
    mfxU16      NumRefLayers;       // 0-1-default, 2+ use given
    mfxU16      ConstQpOffset;      // allows setting negative QPs for 10bit: finalQP[IPB] = mfx.QP[IPB] - ConstQpOffset
    mfxU16      SplitThresholdMultiplier; //0-10-default: multipler = SplitThresholdMultiplier / 10.0
    mfxU16      EnableCmBiref;      // 0-default 1-enables Interpolation and GpuBiref 
    mfxU16      reserved[36];       // 256 bytes total} mfxExtCodingOptionHEVC;
} mfxExtCodingOptionHEVC;


#if defined (ADVANCED_REF)

#define MFX_EXTBUFF_AVC_REFLISTS MFX_MAKEFOURCC('R','L','T','S')


#endif // ADVANCED_REF

#define MFX_EXTBUFF_GPU_HANG MFX_MAKEFOURCC('H','A','N','G')
typedef struct {
    mfxExtBuffer Header;
} mfxExtIntGPUHang;

#endif // __MFX_EXT_BUFFERS_H__
