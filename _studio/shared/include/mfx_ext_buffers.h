// Copyright (c) 2008-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __MFX_EXT_BUFFERS_H__
#define __MFX_EXT_BUFFERS_H__

#include "mfxcommon.h"
#include "mfx_config.h"
#include "vm_strings.h"

#ifdef MFX_DEBUG_TOOLS
// internal(undocumented) handles for VideoCORE::SetHandle
#define MFX_HANDLE_TIMING_LOG       ((mfxHandleType)1001)
#define MFX_HANDLE_EXT_OPTIONS      ((mfxHandleType)1002)
#define MFX_HANDLE_TIMING_SUMMARY   ((mfxHandleType)1003)
#define MFX_HANDLE_TIMING_TAL       ((mfxHandleType)1004)
#endif

#ifndef MFX_ADAPTIVE_PLAYBACK_DISABLE
#define MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK MFX_MAKEFOURCC('A','P','B','K')
#endif

#define MFX_EXTBUFF_DDI MFX_MAKEFOURCC('D','D','I','P')

typedef struct {
    mfxExtBuffer Header;

    // parameters below doesn't exist in MediaSDK public API
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

    struct {
        mfxU16 IntraPredBlockSize;  // mask of 1=4x4, 2=8x8, 4=16x16, 8=PCM
        mfxU16 InterPredBlockSize;  // mask of 1=16x16, 2=16x8, 4=8x16, 8=8x8, 16=8x4, 32=4x8, 64=4x4
    } DDI;

    // MediaSDK parametrization
    mfxU16  BRCPrecision;   // 0=default=normal, 1=lowest, 2=normal, 3=highest
    mfxU16  RefRaw;         // (tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON) on=vme reference on raw(input) frames, off=reconstructed frames
    mfxU16  reserved0;
    mfxU16  ConstQP;        // disable bit-rate control and use constant QP
    mfxU16  GlobalSearch;   // 0=default, 1=long, 2=medium, 3=short
    mfxU16  LocalSearch;    // 0=default, 1=type, 2=small, 3=square, 4=diamond, 5=large diamond, 6=exhaustive, 7=heavy horizontal, 8=heavy vertical

    mfxU16 EarlySkip;       // 0=default (let driver choose), 1=enabled, 2=disabled
    mfxU16 LaScaleFactor;   // 0=default (let msdk choose), 1=1x, 2=2x, 4=4x; Deprecated for legacy H264 encoder, for legacy use mfxExtCodingOption2::LookAheadDS instead
    mfxU16 IBC;             // on/off - SCC IBC Mode
    mfxU16 Palette;         // on/off - SCC Palette Mode
    mfxU16 StrengthN;       // strength=StrengthN/100.0
    mfxU16 FractionalQP;    // 0=disabled (default), 1=enabled

    mfxU16 NumActiveRefP;   //
    mfxU16 NumActiveRefBL0; //

    mfxU16 DisablePSubMBPartition;  // tri-state, default depends on Profile and Level
    mfxU16 DisableBSubMBPartition;  // tri-state, default depends on Profile and Level
    mfxU16 WeightedBiPredIdc;       // 0 - off, 1 - explicit (unsupported), 2 - implicit
    mfxU16 DirectSpatialMvPredFlag; // (tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON)on=spatial on, off=temporal on
    mfxU16 Transform8x8Mode;        // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON
    mfxU16 reserved3;       // 0..255
    mfxU16 LongStartCodes;          // tri-state, use long start-codes for all NALU
    mfxU16 CabacInitIdcPlus1;       // 0 - use default value, 1 - cabac_init_idc = 0 and so on
    mfxU16 NumActiveRefBL1;         //
    mfxU16 QpUpdateRange;           //
    mfxU16 RegressionWindow;        //
    mfxU16 LookAheadDependency;     // LookAheadDependency < LookAhead
    mfxU16 Hme;                     // tri-state
    mfxU16 reserved4;               //
    mfxU16 WriteIVFHeaders;         // tri-state
    mfxU16 RefreshFrameContext;
    mfxU16 ChangeFrameContextIdxForTS;
    mfxU16 SuperFrameForTS;
    mfxU16 QpAdjust;                // on/off - forces sps.QpAdjustment
    mfxU16 TMVP;                    // tri-state: 0, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON

} mfxExtCodingOptionDDI;

#ifndef OPEN_SOURCE
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
    mfxU16      Log2MinCuQpDeltaSize; // 6-64x64; 5-32x32; 4-16x16 3-8x8
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
    mfxU16      DeltaQpMode;        // DeltaQP modes: 0-default 1-disabled; 1+(0x1 = CAQ, 0x2 = CAL, 0x4 = PAQ, 0x8 = PSY, 0x10 = HROI)
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
    mfxU16      RepackForMaxFrameSize;
    mfxU16      AutoScaleToCoresUsingTiles; // Automatically Scale to detected cores using tiles if needed.
    mfxU16      MaxTaskChainEnc;
    mfxU16      MaxTaskChainInloop;
    mfxU16      reserved[31];       // 256 bytes total} mfxExtCodingOptionHEVC;
} mfxExtCodingOptionHEVC;

#define MFX_EXTBUFF_AV1ENC MFX_MAKEFOURCC('E','A','V','1')
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
    mfxU16      Log2MinCuQpDeltaSize; // 6-64x64; 5-32x32; 4-16x16 3-8x8
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
    mfxU16      DeltaQpMode;        // DeltaQP modes: 0-default 1-disabled; 1+(0x1 = CAQ, 0x2 = CAL, 0x4 = PAQ, 0x8 = PSY, 0x10 = HROI)
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
    mfxU16      EnableCmInterp;      // 0-default 1-enables Interpolation and GpuBiref
    mfxU16      RepackForMaxFrameSize;
    mfxU16      AutoScaleToCoresUsingTiles; // Automatically Scale to detected cores using tiles if needed.
    mfxU16      MaxTaskChainEnc;
    mfxU16      MaxTaskChainInloop;
    // AV1 stuff
    mfxU16      FwdProbUpdateCoef;      // on/off forward probability update for coefficients
    mfxU16      FwdProbUpdateSyntax;    // on/off forward probability update for other syntax elements
    mfxU16      DeblockingLevelMethod;  // 0-default, 1-QP based, 2-Full search on full pic, 3-Smart search on full pic
    mfxU16      AllowHpMv;              // on/off high precision motion vectors
    mfxU16      MaxTxDepthIntra;        // Number of Tx sizes tested during Intra mode decision: 0-default, 1..4
    mfxU16      MaxTxDepthInter;        // Number of Tx sizes tested during Inter mode decision: 0-default, 1..4
    mfxU16      MaxTxDepthIntraRefine;  // Number of Tx sizes tested during Intra mode refinement: 0-default, 1..4
    mfxU16      MaxTxDepthInterRefine;  // Number of Tx sizes tested during Inter mode refinement: 0-default, 1..4
    mfxU16      ChromaRDO;              // on/off Chroma RDO during mode decision
    mfxU16      InterpFilter;           // on/off Interpolation filters during mode decision
    mfxU16      InterpFilterRefine;     // on/off Interpolation filters during mode refinement
    mfxU16      IntraRDO;               // on/off Intra mode decision by RDO
    mfxU16      InterRDO;               // on/off Inter mode decision by RDO
    mfxU16      IntraInterRDO;          // on/off Inter mode decision by RDO
    mfxU16      CodecTypeExt;           // Codec type: 0-default; 1-VP9; 2-AV1
    mfxU16      CDEF;                   // tri-state
    mfxU16      LRMode;                 // loop restoration tri-state
    mfxU16      SRMode;                 // super resolution tri-state
    mfxU16      CFLMode;                // super resolution tri-state
    mfxU16      ScreenMode;             // screen content tools: 0-off, 1-intrabc, 2-palette, 3-both, 4-auto
    mfxU16      DisableFrameEndUpdateCdf; // 0,1-on, 2-off
    mfxU16      NumTileColumnsKeyFrame;   // 0-default, 1..N-num tile columns
    mfxU16      NumTileColumnsInterFrame; // 0-default, 1..N-num tile columns
    mfxU16      NumGpuSlices;
    mfxU16      reserved[7];           // 256 bytes total} mfxExtCodingOptionHEVC;
} mfxExtCodingOptionAV1E;


#ifdef MFX_EXTBUFF_FORCE_PRIVATE_DDI_ENABLE
#define MFX_EXTBUFF_FORCE_PRIVATE_DDI MFX_MAKEFOURCC('F','P','R','V')

typedef struct {
    mfxExtBuffer Header;
} mfxExtForcePrivateDDI;
#endif // #ifdef MFX_EXTBUFF_FORCE_PRIVATE_DDI_ENABLE

#endif // #ifndef OPEN_SOURCE

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
#define MFX_EXTBUFF_GPU_HANG MFX_MAKEFOURCC('H','A','N','G')

typedef struct {
    mfxExtBuffer Header;
} mfxExtIntGPUHang;
#endif

#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
#define MFX_EXTBUFF_LP_LOOKAHEAD MFX_MAKEFOURCC('L','P','L','A')
typedef struct {
    mfxExtBuffer Header;
    mfxU16       LookAheadDepth;
    mfxU16       InitialDelayInKB;
    mfxU16       BufferSizeInKB;
    mfxU16       TargetKbps;
    mfxU8        LookAheadScaleX;
    mfxU8        LookAheadScaleY;
    mfxU16       GopRefDist;
    mfxU16       MaxAdaptiveGopSize;
    mfxU16       MinAdaptiveGopSize;
    mfxU32       codecTypeInEncodePass;
    mfxU16       reserved[18];
} mfxExtLplaParam;

#define MFX_EXTBUFF_LPLA_STATUS MFX_MAKEFOURCC('L', 'P', 'L', 'S')
typedef struct {
    mfxExtBuffer    Header;
    mfxU32          StatusReportFeedbackNumber;
    mfxU8           CqmHint; // Custom quantization matrix hint. 0x00 - flat matrix; 0x01 - CQM; 0xFF - invalid hint; other values are reserved.
    mfxU8           IntraHint;
    mfxU8           MiniGopSize;
    mfxU8           QpModulationStrength;
    mfxU32          TargetFrameSize;
    mfxU32          TargetBufferFullnessInBit;
    mfxU16          reserved[19];
} mfxExtLpLaStatus;
#endif

#endif // __MFX_EXT_BUFFERS_H__
