// Copyright (c) 2014-2019 Intel Corporation
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

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_defs.h"
#include "mfx_av1_defaults.h"

using namespace AV1Enc::MfxEnumShortAliases;

#define TU_OPT_SW(opt, t1, t2, t3, t4, t5, t6, t7)   static const mfxU16 tab_sw_##opt[] = {t1, t2, t3, t4, t5, t6, t7}
#define TU_OPT_GACC(opt, t1, t2, t3, t4, t5, t6, t7) static const mfxU16 tab_gacc_##opt[] = {t1, t2, t3, t4, t5, t6, t7};
#define TU_OPT_ALL(opt, t1, t2, t3, t4, t5, t6, t7) \
    TU_OPT_SW(opt, t1, t2, t3, t4, t5, t6, t7);     \
    TU_OPT_GACC(opt, t1, t2, t3, t4, t5, t6, t7)

#define TAB_TU(mode, x) {{MFX_EXTBUFF_AV1ENC, sizeof(mfxExtCodingOptionAV1E)}, \
    tab_##mode##_Log2MaxCUSize[x],\
    tab_##mode##_MaxCUDepth[x],\
    tab_##mode##_QuadtreeTULog2MaxSize[x],\
    tab_##mode##_QuadtreeTULog2MinSize[x],\
    tab_##mode##_QuadtreeTUMaxDepthIntra[x],\
    tab_##mode##_QuadtreeTUMaxDepthInter[x],\
    tab_##mode##_QuadtreeTUMaxDepthInterRD[x],\
    tab_##mode##_AnalyzeChroma[x],\
    tab_##mode##_SignBitHiding[x],\
    tab_##mode##_RDOQuant[x],\
    tab_##mode##_SAO[x],\
    tab_##mode##_SplitThresholdStrengthCUIntra[x],\
    tab_##mode##_SplitThresholdStrengthTUIntra[x],\
    tab_##mode##_SplitThresholdStrengthCUInter[x],\
    tab_##mode##_IntraNumCand1_2[x],\
    tab_##mode##_IntraNumCand1_3[x],\
    tab_##mode##_IntraNumCand1_4[x],\
    tab_##mode##_IntraNumCand1_5[x],\
    tab_##mode##_IntraNumCand1_6[x],\
    tab_##mode##_IntraNumCand2_2[x],\
    tab_##mode##_IntraNumCand2_3[x],\
    tab_##mode##_IntraNumCand2_4[x],\
    tab_##mode##_IntraNumCand2_5[x],\
    tab_##mode##_IntraNumCand2_6[x],\
    tab_##mode##_WPP[x],\
    tab_##mode##_reserved[x],\
    tab_##mode##_PartModes[x],\
    tab_##mode##_CmIntraThreshold[x],\
    tab_##mode##_TUSplitIntra[x],\
    tab_##mode##_CUSplit[x],\
    tab_##mode##_IntraAngModes[x],\
    tab_##mode##_EnableCm[x],\
    tab_##mode##_BPyramid[x],\
    tab_##mode##_reserved[x],\
    tab_##mode##_HadamardMe[x],\
    tab_##mode##_TMVP[x],\
    tab_##mode##_Deblocking[x],\
    tab_##mode##_RDOQuantChroma[x],\
    tab_##mode##_RDOQuantCGZ[x],\
    tab_##mode##_SaoOpt[x],\
    tab_##mode##_SaoSubOpt[x],\
    tab_##mode##_IntraNumCand0_2[x],\
    tab_##mode##_IntraNumCand0_3[x],\
    tab_##mode##_IntraNumCand0_4[x],\
    tab_##mode##_IntraNumCand0_5[x],\
    tab_##mode##_IntraNumCand0_6[x],\
    tab_##mode##_CostChroma[x],\
    tab_##mode##_PatternIntPel[x],\
    tab_##mode##_FastSkip[x],\
    tab_##mode##_PatternSubPel[x],\
    tab_##mode##_ForceNumThread[x],\
    tab_##mode##_FastCbfMode[x],\
    tab_##mode##_PuDecisionSatd[x],\
    tab_##mode##_MinCUDepthAdapt[x],\
    tab_##mode##_MaxCUDepthAdapt[x],\
    tab_##mode##_NumBiRefineIter[x],\
    tab_##mode##_CUSplitThreshold[x],\
    tab_##mode##_DeltaQpMode[x],\
    tab_##mode##_Enable10bit[x],\
    tab_##mode##_IntraAngModesP[x],\
    tab_##mode##_IntraAngModesBRef[x],\
    tab_##mode##_IntraAngModesBnonRef[x],\
    tab_##mode##_IntraChromaRDO[x],\
    tab_##mode##_FastInterp[x],\
    tab_##mode##_SplitThresholdTabIndex[x],\
    tab_##mode##_CpuFeature[x],\
    tab_##mode##_TryIntra[x],\
    tab_##mode##_FastAMPSkipME[x],\
    tab_##mode##_FastAMPRD[x],\
    tab_##mode##_SkipMotionPartition[x],\
    tab_##mode##_SkipCandRD[x],\
    tab_##mode##_FramesInParallel[x],\
    tab_##mode##_AdaptiveRefs[x],\
    tab_##mode##_FastCoeffCost[x],\
    tab_##mode##_NumRefFrameB[x],\
    tab_##mode##_IntraMinDepthSC[x],\
    tab_##mode##_InterMinDepthSTC[x],\
    tab_##mode##_MotionPartitionDepth[x],\
    tab_##mode##_reserved[x],\
    tab_##mode##_AnalyzeCmplx[x],\
    tab_##mode##_RateControlDepth[x],\
    tab_##mode##_LowresFactor[x],\
    tab_##mode##_DeblockBorders[x],\
    tab_##mode##_SAOChroma[x],\
    tab_##mode##_RepackProb[x],\
    tab_##mode##_NumRefLayers[x],\
    tab_##mode##_ConstQpOffset[x],\
    tab_##mode##_SplitThresholdMultiplier[x],\
    tab_##mode##_EnableCmInterp[x],\
    tab_##mode##_RepackForMaxFrameSize[x], \
    tab_##mode##_AutoScaleToCoresUsingTiles[x], \
    tab_##mode##_MaxTaskChainEnc[x], \
    tab_##mode##_MaxTaskChainInloop[x], \
    tab_##mode##_FwdProbUpdateCoef[x],\
    tab_##mode##_FwdProbUpdateSyntax[x],\
    tab_##mode##_DeblockingLevelMethod[x],\
    tab_##mode##_AllowHpMv[x],\
    tab_##mode##_MaxTxDepthIntra[x],\
    tab_##mode##_MaxTxDepthInter[x],\
    tab_##mode##_MaxTxDepthIntraRefine[x],\
    tab_##mode##_MaxTxDepthInterRefine[x],\
    tab_##mode##_ChromaRDO[x],\
    tab_##mode##_InterpFilter[x],\
    tab_##mode##_InterpFilterRefine[x],\
    tab_##mode##_IntraRDO[x],\
    tab_##mode##_InterRDO[x],\
    tab_##mode##_IntraInterRDO[x],\
    tab_##mode##_CodecType[x],\
    tab_##mode##_CDEF[x],\
    tab_##mode##_LRMode[x],\
    tab_##mode##_SRMode[x],\
    tab_##mode##_CFLMode[x],\
    tab_##mode##_ScreenMode[x],\
    tab_##mode##_DisableFrameEndUpdateCdf[x],\
    }

    // Extended bit depth
    TU_OPT_SW  (Enable10bit,                  OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_GACC(Enable10bit,                  OFF, OFF, OFF, OFF, OFF, OFF, OFF);

    // Mode decision CU/TU
    TU_OPT_ALL (QuadtreeTULog2MaxSize,          5,   5,   5,   5,   5,   5,   5);
    TU_OPT_ALL (QuadtreeTULog2MinSize,          2,   2,   2,   2,   2,   2,   2);

    TU_OPT_ALL (MaxTxDepthIntra,                4,   1,   1,   1,   1,   1,   1);
    TU_OPT_ALL (MaxTxDepthInter,                4,   1,   1,   1,   1,   1,   1);
    TU_OPT_ALL (MaxTxDepthIntraRefine,          1,   4,   4,   4,   4,   4,   4);
    TU_OPT_ALL (MaxTxDepthInterRefine,          1,   4,   4,   4,   4,   4,   4);

    TU_OPT_SW  (Log2MaxCUSize,                  6,   6,   6,   6,   6,   6,   6);
    TU_OPT_SW  (MaxCUDepth,                     4,   4,   4,   4,   4,   4,   4);

    TU_OPT_SW  (QuadtreeTUMaxDepthIntra,        4,   3,   2,   2,   2,   2,   2);


    TU_OPT_SW  (QuadtreeTUMaxDepthInter,        3,   3,   2,   2,   2,   2,   2);
    TU_OPT_SW  (QuadtreeTUMaxDepthInterRD,      3,   3,   2,   2,   2,   1,   1);


    TU_OPT_GACC(Log2MaxCUSize,                  6,   6,   6,   6,   6,   6,   6);
    TU_OPT_GACC(MaxCUDepth,                     4,   4,   4,   4,   4,   4,   4);
    TU_OPT_GACC(QuadtreeTUMaxDepthIntra,        4,   3,   2,   2,   2,   2,   2);
    TU_OPT_GACC(QuadtreeTUMaxDepthInter,        3,   3,   2,   2,   2,   2,   2);
    TU_OPT_GACC(QuadtreeTUMaxDepthInterRD,      3,   3,   2,   2,   2,   1,   1);
    TU_OPT_ALL (TUSplitIntra,                   1,   1,   3,   3,   3,   3,   3);

    TU_OPT_SW  (CUSplit,                        2,   2,   2,   2,   2,   2,   2);
    TU_OPT_GACC(CUSplit,                        1,   1,   1,   1,   1,   1,   1);
    TU_OPT_ALL (PuDecisionSatd,               OFF, OFF, OFF, OFF, OFF, OFF, OFF);

    TU_OPT_ALL (MinCUDepthAdapt,              OFF, OFF,  ON,  ON,  ON,  OFF, OFF);

    TU_OPT_SW  (MaxCUDepthAdapt,               ON,  ON,  ON,  ON,  ON,   ON,  ON);
    TU_OPT_GACC(MaxCUDepthAdapt,               ON,  ON,  ON,  ON,  ON,  OFF, OFF);

    TU_OPT_SW  (CUSplitThreshold,               0,   0,   0,   0,   0,   0,   0);

    TU_OPT_GACC(CUSplitThreshold,               0,   0,   0,   0,   0,   0,   0);
    TU_OPT_ALL (PartModes,                      3,   2,   2,   2,   1,   1,   1);
    TU_OPT_SW  (FastSkip,                     OFF, OFF, OFF, OFF, OFF,  ON,  ON);
    TU_OPT_GACC(FastSkip,                     OFF, OFF, OFF, OFF, OFF,  ON,  ON);
    TU_OPT_ALL (FastCbfMode,                  OFF, OFF,  ON,  ON,  ON,  ON,  ON);

    TU_OPT_SW  (SplitThresholdStrengthCUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_SW  (SplitThresholdStrengthTUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_SW  (SplitThresholdStrengthCUInter,  1,   2,   2,   3,   3,   3,   3);

    TU_OPT_GACC(SplitThresholdStrengthCUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_GACC(SplitThresholdStrengthTUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_GACC(SplitThresholdStrengthCUInter,  1,   2,   2,   3,   3,   3,   3);

    TU_OPT_SW  (SplitThresholdTabIndex       ,  1,   1,   1,   1,   1,   1,   1); //Tab1 + strength 3 - fastest combination
    TU_OPT_GACC(SplitThresholdTabIndex       ,  1,   1,   1,   1,   1,   1,   1);

    TU_OPT_SW  (SplitThresholdMultiplier,      10,  10,  10,  10,  10,  10,  10);
    TU_OPT_GACC(SplitThresholdMultiplier,      10,  10,  10,  10,  10,  10,  10);

    //Chroma analysis
    TU_OPT_ALL (AnalyzeChroma,                 ON,  ON,  ON,  ON,  ON, OFF, OFF);
    TU_OPT_SW  (CostChroma,                    ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_GACC(CostChroma,                    ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_ALL (ChromaRDO,                     ON, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (IntraRDO,                      ON, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (InterRDO,                      ON, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (IntraInterRDO,                 ON, OFF, OFF, OFF, OFF, OFF, OFF);

    TU_OPT_ALL (IntraChromaRDO,                ON,  ON,  ON,  ON,  ON,  ON,  ON);

    TU_OPT_ALL (reserved,                       0,   0,   0,   0,   0,   0,   0);

    //Filtering
    TU_OPT_ALL  (SAO,                           ON,  ON,  ON,  ON,   ON,  ON,  ON);
    TU_OPT_ALL  (SAOChroma,                     ON,  ON,  ON,  ON,  OFF, OFF, OFF);

    TU_OPT_SW  (SaoOpt,                         1,   1,   2,   2,   2,   2,   2);
    TU_OPT_GACC(SaoOpt,                         1,   1,   1,   1,   2,   2,   2);

    TU_OPT_SW  (SaoSubOpt,                      1,   1,   1,   1,   1,   3,   3);
    TU_OPT_GACC(SaoSubOpt,                      1,   1,   1,   1,   1,   3,   3);
    TU_OPT_ALL (Deblocking,                    ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_ALL (DeblockBorders,                ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_ALL (DeblockingLevelMethod,          3,   1,   1,   1,   1,   1,   1);

    //Intra prediction optimization
    TU_OPT_SW  (IntraAngModes,                  1,   1,   1,   1,   1,   1,   1); //I slice SW
    TU_OPT_GACC(IntraAngModes,                  1,   1,   1,   1,   1,   1,   1); //I slice Gacc
    TU_OPT_SW  (IntraAngModesP,                 1,   1,   2,   2,   3,   3,   3); //P slice SW

    TU_OPT_SW  (IntraAngModesBRef,              1,   1,   2,   2,   3,   99,  99); //B Ref slice SW

    TU_OPT_GACC(IntraAngModesP,                 1,   1,   2,   2,   3,   3,   3); //P slice Gacc

    TU_OPT_GACC(IntraAngModesBRef,              1,   1,   2,   2,   3,   99, 99); //B Ref slice Gacc

    TU_OPT_SW  (IntraAngModesBnonRef,           1,   1,   2,  99,  99, 100, 100); //B non Ref slice SW
    TU_OPT_GACC(IntraAngModesBnonRef,           1,   1,   2,  99,  99, 100, 100); //B non Ref slice Gacc

    //Quantization optimization
    TU_OPT_SW  (SignBitHiding,                  ON,  ON,  ON,  ON,  ON, OFF, OFF);
    TU_OPT_GACC(SignBitHiding,                  ON,  ON,  ON,  ON,  ON, OFF, OFF);
    TU_OPT_SW  (RDOQuant,                       ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_ALL (FastCoeffCost,                 OFF,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_SW  (RDOQuantChroma,                 ON,  ON,  ON, OFF, OFF, OFF, OFF);

    TU_OPT_SW  (RDOQuantCGZ,                    ON,  ON,  ON,  ON,  ON,  ON,  ON);

    TU_OPT_GACC(RDOQuant,                       ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_GACC(RDOQuantChroma,                 ON,  ON,  ON, OFF, OFF, OFF, OFF);


    TU_OPT_GACC(RDOQuantCGZ,                    ON,  ON,  ON,  ON,  ON,  ON,  ON);

    TU_OPT_SW  (DeltaQpMode,                    8,   8,   2,   2,   2,   2,   2);
    TU_OPT_GACC(DeltaQpMode,                    2,   2,   2,   2,   2,   2,   1);


    //Intra RDO
    TU_OPT_SW  (IntraNumCand0_2,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand0_3,                2,   2,   2,   2,   2,   2,   2);
    TU_OPT_SW  (IntraNumCand0_4,                2,   2,   2,   2,   2,   2,   2);
    TU_OPT_SW  (IntraNumCand0_5,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand0_6,                1,   1,   1,   1,   1,   1,   1);

    TU_OPT_SW  (IntraNumCand1_2,                6,   6,   4,   2,   2,   1,   1);
    TU_OPT_SW  (IntraNumCand1_3,                6,   6,   4,   2,   2,   2,   2);
    TU_OPT_SW  (IntraNumCand1_4,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand1_5,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand1_6,                4,   3,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_2,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_3,                2,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_4,                2,   2,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_5,                2,   2,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_6,                2,   2,   1,   1,   1,   1,   1);

    TU_OPT_GACC(IntraNumCand0_2,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand0_3,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand0_4,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand0_5,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand0_6,                0,   0,   0,   0,   0,   0,   0);
    TU_OPT_GACC(IntraNumCand1_2,                6,   6,   4,   2,   2,   1,   1);
    TU_OPT_GACC(IntraNumCand1_3,                6,   6,   4,   2,   2,   2,   2);
    TU_OPT_GACC(IntraNumCand1_4,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand1_5,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand1_6,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_2,                3,   3,   2,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_3,                3,   3,   2,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_4,                2,   2,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_5,                2,   2,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_6,                2,   2,   1,   1,   1,   1,   1);

    //GACC
    TU_OPT_SW  (EnableCm,                       OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_GACC(EnableCm,                        ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_SW  (CmIntraThreshold,                 0,   0,   0,   0,   0,   0,   0);
    TU_OPT_GACC(CmIntraThreshold,               576, 576, 576, 576, 576, 576, 576);
    TU_OPT_SW  (EnableCmInterp,                  OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_GACC(EnableCmInterp,                   ON,  ON,  ON,  ON,  ON, ON, ON);

    //Multithreading & optimizations
    TU_OPT_ALL (WPP,                          UNK, UNK, UNK, UNK, UNK, UNK, UNK);
    TU_OPT_ALL (ForceNumThread,                 0,   0,   0,   0,   0,   0,   0);
    TU_OPT_ALL (CpuFeature,                     0,   0,   0,   0,   0,   0,   0);

    //Inter prediction
    TU_OPT_ALL (TMVP,                          ON,  ON,  ON,  ON,  ON,  ON,  ON);

    TU_OPT_SW  (HadamardMe,                     2,   2,   2,   2,   2,   2,   2);

    TU_OPT_GACC(HadamardMe,                     1,   1,   1,   1,   1,   1,   1);
    TU_OPT_ALL (PatternIntPel,                  1,   1,   1,   1,   1,   1,   1);

    TU_OPT_SW  (PatternSubPel,                  6,   6,   6,   6,   6,   6,   6); //4 -dia subpel search; 3- square; 6- fast box + dia orth (see enum SUBPEL_*)
    TU_OPT_GACC(PatternSubPel,                  3,   3,   3,   3,   4,   4,   4); //4 -dia subpel search; 3- square (see enum SUBPEL_*)
    TU_OPT_ALL (AllowHpMv,                     ON, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (InterpFilter,                  ON, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (InterpFilterRefine,           OFF,  ON,  ON,  ON,  ON,  ON,  ON);

    TU_OPT_ALL (NumBiRefineIter,                2,   1,   1,   1,   1,   1,   1); //999-practically infinite iteration
    TU_OPT_ALL (FastInterp,                   OFF, OFF, OFF, OFF, OFF, OFF, OFF);

    TU_OPT_SW  (TryIntra,                       2,   2,   2,   2,   2,   2,   2);


    TU_OPT_GACC(TryIntra,                       2,   2,   2,   2,   2,   2,   2);

    TU_OPT_SW  (FastAMPSkipME,                  2,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(FastAMPSkipME,                  1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (FastAMPRD,                      2,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(FastAMPRD,                      1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (SkipMotionPartition,            2,   2,   2,   1,   1,   1,   1);
    TU_OPT_GACC(SkipMotionPartition,            1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (SkipCandRD,                    ON,  ON,  ON, OFF, OFF, OFF, OFF);
    TU_OPT_GACC(SkipCandRD,                   OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (FramesInParallel,               0,   0,   0,   0,   0,   0,   0);
    //GOP structure, reference options

    TU_OPT_ALL (BPyramid,                      ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_ALL (AdaptiveRefs,                  ON,  ON,  ON,  ON,  ON,  ON,  ON);

    TU_OPT_ALL (NumRefFrameB,                   2,   2,   2,   2,   2,   2,   2);
    TU_OPT_ALL (IntraMinDepthSC,               11,  11,  11,   6,   6,   3,   3);
    TU_OPT_ALL (InterMinDepthSTC,               6,   6,   3,   3,   2,   2,   2);
    TU_OPT_ALL (MotionPartitionDepth,           6,   6,   3,   1,   1,   1,   1);

    TU_OPT_ALL (AnalyzeCmplx,                   0,   0,   0,   0,   0,   0,   0);
    TU_OPT_ALL (RateControlDepth,               0,   0,   0,   0,   0,   0,   0);
    TU_OPT_ALL (LowresFactor,                   3,   3,   3,   3,   3,   3,   3);

    TU_OPT_ALL (RepackProb,                     0,   0,   0,   0,   0,   0,   0);

    TU_OPT_ALL (NumRefLayers,                   4,   4,   4,   4,   4,   4,   4);

    TU_OPT_SW  (ConstQpOffset,                  0,   0,   0,   0,   0,   0,   0);
    TU_OPT_GACC(ConstQpOffset,                  0,   0,   0,   0,   0,   0,   0);

    TU_OPT_ALL (FwdProbUpdateCoef,             ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_ALL (FwdProbUpdateSyntax,           ON,  ON,  ON,  ON,  ON,  ON,  ON);

    TU_OPT_ALL (CodecType,                      1,   1,   1,   1,   1,   1,   1);
    TU_OPT_ALL (CDEF,                          ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_ALL(LRMode,                        OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL(SRMode,                        OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL(CFLMode,                        ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_ALL(ScreenMode,                      4,   4,   4,   4,   4,   4,   4);
    TU_OPT_ALL(DisableFrameEndUpdateCdf,      OFF, OFF, OFF, OFF, OFF, OFF, OFF);

    TU_OPT_ALL(RepackForMaxFrameSize,          ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_SW(AutoScaleToCoresUsingTiles,     OFF, OFF, OFF, OFF, OFF,  ON,  ON);
    TU_OPT_GACC(AutoScaleToCoresUsingTiles,   OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL(MaxTaskChainEnc,                 1,   1,   1,   1,   1,   1,   1);
    TU_OPT_ALL(MaxTaskChainInloop,              1,   1,   1,   1,   1,   1,   1);

namespace AV1Enc {

    const uint8_t tab_defaultNumRefFrameSw[8]   = {2, 4, 4, 4, 4, 4, 4, 4};
    const uint8_t tab_defaultNumRefFrameGacc[8] = {4, 4, 4, 4, 4, 4, 4, 4};

    const uint8_t tab_defaultNumRefFrameLowDelaySw[8]   = {2, 4, 4, 4, 3, 3, 3, 2};
    const uint8_t tab_defaultNumRefFrameLowDelayGacc[8] = {4, 4, 4, 4, 4, 4, 4, 3};

    const mfxExtCodingOptionAV1E tab_defaultOptHevcSw[8] = {
        TAB_TU(sw, 3), TAB_TU(sw, 0), TAB_TU(sw, 1), TAB_TU(sw, 2), TAB_TU(sw, 3), TAB_TU(sw, 4), TAB_TU(sw, 5), TAB_TU(sw, 6)
    };

    const mfxExtCodingOptionAV1E tab_defaultOptHevcGacc7 = TAB_TU(gacc, 6);
    const mfxExtCodingOptionAV1E tab_defaultOptHevcGacc[8] = {
        TAB_TU(gacc, 3), TAB_TU(gacc, 0), TAB_TU(gacc, 1), TAB_TU(gacc, 2), TAB_TU(gacc, 3), TAB_TU(gacc, 4), TAB_TU(gacc, 5), TAB_TU(gacc, 6)
    };

#ifdef MFX_VA
    const uint8_t DEFAULT_ENABLE_CM = ON;
#else
    const uint8_t DEFAULT_ENABLE_CM = OFF;
#endif
};


#endif // MFX_ENABLE_H265_VIDEO_ENCODE2
