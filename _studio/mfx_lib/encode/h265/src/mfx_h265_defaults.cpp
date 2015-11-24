//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2015 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_defaults.h"

using namespace H265Enc::MfxEnumShortAliases;

#define TU_OPT_SW(opt, t1, t2, t3, t4, t5, t6, t7)   static const mfxU16 tab_sw_##opt[] = {t1, t2, t3, t4, t5, t6, t7}
#define TU_OPT_GACC(opt, t1, t2, t3, t4, t5, t6, t7) static const mfxU16 tab_gacc_##opt[] = {t1, t2, t3, t4, t5, t6, t7};
#define TU_OPT_ALL(opt, t1, t2, t3, t4, t5, t6, t7) \
    TU_OPT_SW(opt, t1, t2, t3, t4, t5, t6, t7);     \
    TU_OPT_GACC(opt, t1, t2, t3, t4, t5, t6, t7)

#define TAB_TU(mode, x) {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, \
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
    }

    // Extended bit depth
    TU_OPT_SW  (Enable10bit,                  OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_GACC(Enable10bit,                  OFF, OFF, OFF, OFF, OFF, OFF, OFF);

    // Mode decision CU/TU 
    TU_OPT_ALL (QuadtreeTULog2MaxSize,          5,   5,   5,   5,   5,   5,   5);
    TU_OPT_ALL (QuadtreeTULog2MinSize,          2,   2,   2,   2,   2,   2,   2);
#ifdef AMT_VQ_TU
    TU_OPT_SW  (Log2MaxCUSize,                  6,   6,   6,   6,   6,   6,   6);
    TU_OPT_SW  (MaxCUDepth,                     4,   4,   4,   4,   4,   4,   4);
#else
    TU_OPT_SW  (Log2MaxCUSize,                  6,   6,   5,   5,   5,   5,   5);
    TU_OPT_SW  (MaxCUDepth,                     4,   4,   3,   3,   3,   3,   3);
#endif
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    TU_OPT_SW  (QuadtreeTUMaxDepthIntra,        4,   3,   2,   2,   2,   2,   2);
#else
    TU_OPT_SW  (QuadtreeTUMaxDepthIntra,        4,   3,   2,   2,   2,   1,   1);
#endif

#if defined(AMT_ADAPTIVE_TU_DEPTH)
    TU_OPT_SW  (QuadtreeTUMaxDepthInter,        3,   3,   2,   2,   2,   2,   2);
    TU_OPT_SW  (QuadtreeTUMaxDepthInterRD,      3,   3,   2,   2,   2,   1,   1);
#else
    TU_OPT_SW  (QuadtreeTUMaxDepthInter,        3,   3,   2,   2,   2,   1,   1);
    TU_OPT_SW  (QuadtreeTUMaxDepthInterRD,      3,   3,   2,   2,   2,   1,   1);
#endif

    TU_OPT_GACC(Log2MaxCUSize,                  6,   6,   6,   6,   6,   6,   6);
    TU_OPT_GACC(MaxCUDepth,                     4,   4,   4,   4,   4,   4,   4);
    TU_OPT_GACC(QuadtreeTUMaxDepthIntra,        4,   3,   2,   2,   2,   2,   2);
    TU_OPT_GACC(QuadtreeTUMaxDepthInter,        3,   3,   2,   2,   2,   2,   2);
    TU_OPT_GACC(QuadtreeTUMaxDepthInterRD,      3,   3,   2,   2,   2,   1,   1);
    TU_OPT_ALL (TUSplitIntra,                   1,   1,   3,   3,   3,   3,   3);

    TU_OPT_ALL (CUSplit,                        2,   2,   2,   2,   2,   2,   2);
    TU_OPT_ALL (PuDecisionSatd,               OFF, OFF, OFF, OFF, OFF, OFF, OFF);
#ifdef AMT_VQ_TU
    TU_OPT_ALL (MinCUDepthAdapt,              OFF, OFF,  ON,  ON,  ON,  ON,  ON);
#else
    TU_OPT_ALL (MinCUDepthAdapt,              OFF, OFF, OFF,  ON,  ON,  ON,  ON);
#endif
    TU_OPT_SW  (MaxCUDepthAdapt,               ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_GACC(MaxCUDepthAdapt,               ON,  ON,  ON,  ON,  ON,  ON,  ON);
#ifdef AMT_THRESHOLDS
#ifdef AMT_VQ_TUNE
    TU_OPT_SW  (CUSplitThreshold,               0,   0,   0,   0,   0,   0,   0);
#else
    TU_OPT_SW  (CUSplitThreshold,               0,   0,   0,  64, 192, 224, 240);
#endif
#else
    TU_OPT_SW  (CUSplitThreshold,               0,  64, 128, 192, 192, 224, 256);
#endif
    TU_OPT_GACC(CUSplitThreshold,               0,   0,   0,   0,   0,   0,   0);
    TU_OPT_SW  (PartModes,                      3,   2,   2,   1,   1,   1,   1);
    TU_OPT_GACC(PartModes,                      1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (FastSkip,                     OFF, OFF, OFF, OFF, OFF,  ON,  ON);
    TU_OPT_GACC(FastSkip,                     OFF, OFF, OFF, OFF, OFF,  ON,  ON);
    TU_OPT_ALL (FastCbfMode,                  OFF, OFF,  ON,  ON,  ON,  ON,  ON);

#ifdef AMT_THRESHOLDS
    TU_OPT_SW  (SplitThresholdStrengthCUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_SW  (SplitThresholdStrengthTUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_SW  (SplitThresholdStrengthCUInter,  1,   2,   2,   3,   3,   3,   3);
#else
    TU_OPT_SW  (SplitThresholdStrengthCUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_SW  (SplitThresholdStrengthTUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_SW  (SplitThresholdStrengthCUInter,  1,   1,   1,   1,   2,   3,   3);
#endif
    TU_OPT_GACC(SplitThresholdStrengthCUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_GACC(SplitThresholdStrengthTUIntra,  1,   1,   1,   1,   2,   3,   3);
    TU_OPT_GACC(SplitThresholdStrengthCUInter,  1,   2,   2,   3,   3,   3,   3);
#ifdef AMT_VQ_TU
    TU_OPT_SW  (SplitThresholdTabIndex       ,  1,   1,   1,   1,   1,   1,   1); //Tab1 + strength 3 - fastest combination
    TU_OPT_GACC(SplitThresholdTabIndex       ,  1,   1,   1,   1,   1,   1,   1); 
#else
    TU_OPT_ALL (SplitThresholdTabIndex       ,  1,   1,   1,   1,   1,   1,   1); //Tab1 + strength 3 - fastest combination
#endif

    //Chroma analysis
    TU_OPT_ALL (AnalyzeChroma,                 ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_SW  (CostChroma,                    ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_GACC(CostChroma,                    ON,  ON,  ON,  ON, OFF, OFF, OFF);
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    TU_OPT_ALL (IntraChromaRDO,                ON,  ON,  ON,  ON,  ON,  ON,  ON);
#else
    TU_OPT_ALL (IntraChromaRDO,               OFF, OFF, OFF, OFF, OFF, OFF,  OFF);
#endif
    TU_OPT_ALL (reserved,                       0,   0,   0,   0,   0,   0,   0);

    //Filtering
#ifdef AMT_SAO_MIN
    TU_OPT_ALL  (SAO,                           ON,  ON,  ON,  ON,  ON,  ON, ON);
    TU_OPT_ALL  (SAOChroma,                     ON,  ON,  ON,  ON,  OFF,  OFF, OFF);
#else
    TU_OPT_SW  (SAO,                           ON,  ON,  ON,  ON,  ON,  ON,  OFF);
    TU_OPT_SW  (SAOChroma,                     ON,  ON,  ON,  ON,  OFF,  OFF, OFF);
    TU_OPT_GACC(SAO,                           ON,  ON,  ON,  ON,  ON,  OFF, OFF);
    TU_OPT_GACC(SAOChroma,                     ON,  ON,  ON,  ON,  OFF,  OFF, OFF);
#endif
    
#ifdef AMT_SAO_MIN
    TU_OPT_SW  (SaoOpt,                         1,   1,   2,   2,   2,   2,   2);
    TU_OPT_GACC(SaoOpt,                         1,   1,   1,   1,   2,   2,   2);
#else
    TU_OPT_ALL (SaoOpt,                         1,   1,   2,   2,   2,   2,   2);
#endif
    //TU_OPT_ALL (SaoSubOpt,                      1,   1,   1,   1,   1,   2,   3);
    TU_OPT_SW  (SaoSubOpt,                      1,   1,   1,   1,   1,   2,   3);
    TU_OPT_GACC(SaoSubOpt,                      1,   1,   1,   1,   1,   3,   3);
    TU_OPT_ALL (Deblocking,                    ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_ALL (DeblockBorders,                ON,  ON,  ON,  ON,  ON,  ON,  ON);

    //Intra prediction optimization
    TU_OPT_SW  (IntraAngModes,                  1,   1,   1,   1,   1,   1,   1); //I slice SW
    TU_OPT_GACC(IntraAngModes,                  1,   1,   1,   1,   1,   1,   1); //I slice Gacc
    TU_OPT_SW  (IntraAngModesP,                 1,   1,   2,   2,   3,   3,   3); //P slice SW
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    TU_OPT_SW  (IntraAngModesBRef,              1,   1,   2,   2,   3,   3,   99); //B Ref slice SW
#else
    TU_OPT_SW  (IntraAngModesBRef,              1,   1,   2,   2,   3,   3, 100); //B Ref slice SW
#endif
    TU_OPT_GACC(IntraAngModesP,                 1,   1,   2,   2,   3,   3,   3); //P slice Gacc
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    TU_OPT_GACC(IntraAngModesBRef,              1,   1,   2,   2,   3,   3,  99); //B Ref slice Gacc
#else
    TU_OPT_GACC(IntraAngModesBRef,              1,   1,   2,   2,   3,   3, 100); //B Ref slice Gacc
#endif
    TU_OPT_SW  (IntraAngModesBnonRef,           1,   1,   2,  99,  99, 100, 100); //B non Ref slice SW
    TU_OPT_GACC(IntraAngModesBnonRef,           1,   1,   2,  99,  99, 100, 100); //B non Ref slice Gacc

    //Quantization optimization
    TU_OPT_SW  (SignBitHiding,                  ON,  ON,  ON,  ON,  ON,  ON, OFF);
    TU_OPT_GACC(SignBitHiding,                  ON,  ON,  ON,  ON,  ON,  ON, OFF);
    TU_OPT_SW  (RDOQuant,                       ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_SW  (FastCoeffCost,                 OFF, OFF, OFF, OFF,  ON,  ON,  ON);
    TU_OPT_GACC(FastCoeffCost,                 OFF, OFF, OFF, OFF,  ON,  ON,  ON);
    TU_OPT_SW  (RDOQuantChroma,                 ON,  ON,  ON, OFF, OFF, OFF, OFF);
#ifdef AMT_ALT_ENCODE
    TU_OPT_SW  (RDOQuantCGZ,                    ON,  ON,  ON,  ON,  ON,  ON,  ON);
#else
    TU_OPT_SW  (RDOQuantCGZ,                    ON,  ON,  ON,  ON, OFF, OFF, OFF);
#endif
    TU_OPT_GACC(RDOQuant,                       ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_GACC(RDOQuantChroma,                 ON,  ON,  ON, OFF, OFF, OFF, OFF);
    
#ifdef AMT_ALT_ENCODE
    TU_OPT_GACC(RDOQuantCGZ,                    ON,  ON,  ON,  ON,  ON,  ON,  ON);
#else
    TU_OPT_GACC(RDOQuantCGZ,                   OFF, OFF, OFF, OFF, OFF, OFF, OFF);
#endif
    TU_OPT_SW  (DeltaQpMode,                    8,   8,   2,   2,   2,   2,   2);
    TU_OPT_GACC(DeltaQpMode,                    2,   2,   2,   2,   2,   2,   2);


    //Intra RDO
    TU_OPT_SW  (IntraNumCand0_2,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand0_3,                2,   2,   2,   2,   2,   2,   2);
    TU_OPT_SW  (IntraNumCand0_4,                2,   2,   2,   2,   2,   2,   2);
    TU_OPT_SW  (IntraNumCand0_5,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand0_6,                1,   1,   1,   1,   1,   1,   1);
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
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
#else
    TU_OPT_SW  (IntraNumCand1_2,                6,   6,   4,   2,   2,   1,   1);
    TU_OPT_SW  (IntraNumCand1_3,                6,   6,   4,   2,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand1_4,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand1_5,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand1_6,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_2,                3,   3,   2,   2,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_3,                3,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_4,                2,   2,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_5,                2,   2,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_6,                2,   2,   1,   1,   1,   1,   1);
#endif

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
    TU_OPT_SW  (EnableCm,                     OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_GACC(EnableCm,                      ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_SW  (CmIntraThreshold,               0,   0,   0,   0,   0,   0,   0);
    TU_OPT_GACC(CmIntraThreshold,             576, 576, 576, 576, 576, 576, 576);

    //Multithreading & optimizations
    TU_OPT_ALL (WPP,                          UNK, UNK, UNK, UNK, UNK, UNK, UNK);
    TU_OPT_ALL (ForceNumThread,                 0,   0,   0,   0,   0,   0,   0);
    TU_OPT_ALL (CpuFeature,                     0,   0,   0,   0,   0,   0,   0);

    //Inter prediction
    TU_OPT_ALL (TMVP,                          ON,  ON,  ON,  ON,  ON,  ON,  ON);
#if defined(AMT_ALT_FAST_SKIP) || defined(AMT_FAST_SUBPEL_SEARCH)
    TU_OPT_SW  (HadamardMe,                     2,   2,   2,   2,   2,   2,   2);
#else
    TU_OPT_SW  (HadamardMe,                     2,   2,   2,   2,   2,   1,   1);
#endif
    TU_OPT_GACC(HadamardMe,                     2,   2,   2,   2,   2,   2,   2);
    TU_OPT_ALL (PatternIntPel,                  1,   1,   1,   1,   1,   1,   1);
#ifdef AMT_FAST_SUBPEL_SEARCH
    TU_OPT_SW  (PatternSubPel,                  3,   3,   3,   3,   6,   6,   6); //4 -dia subpel search; 3- square; 6- fast box + dia orth (see enum SUBPEL_*)
    TU_OPT_GACC(PatternSubPel,                  3,   3,   3,   3,   4,   4,   4); //4 -dia subpel search; 3- square (see enum SUBPEL_*)
#else
    TU_OPT_ALL (PatternSubPel,                  3,   3,   3,   3,   4,   4,   4); //4 -dia subpel search; 3- square (see enum SUBPEL_*)
#endif
    TU_OPT_SW  (NumBiRefineIter,              999, 999, 999,   3,   2,   1,   1); //999-practically infinite iteration
    TU_OPT_GACC(NumBiRefineIter,              999, 999, 999,   3,   2,   1,   1); //999-practically infinite iteration
    TU_OPT_ALL (FastInterp,                   OFF, OFF, OFF, OFF, OFF, OFF, OFF); 
#ifdef AMT_ALT_ENCODE
    TU_OPT_SW  (TryIntra,                       2,   2,   2,   2,   2,   2,   2);
#else
    TU_OPT_SW  (TryIntra,                       2,   2,   2,   2,   2,   2,   1);
#endif
#ifdef AMT_ALT_ENCODE
    TU_OPT_GACC(TryIntra,                       2,   2,   2,   2,   2,   2,   2);
#else
    TU_OPT_GACC(TryIntra,                       1,   1,   1,   1,   1,   1,   1);
#endif
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

    TU_OPT_SW  (NumRefFrameB,                   0,   0,   3,   3,   2,   2,   2);
    TU_OPT_GACC(NumRefFrameB,                   0,   0,   3,   3,   2,   2,   2);
    TU_OPT_ALL (IntraMinDepthSC,               11,  11,  11,   6,   6,   3,   3);
    TU_OPT_ALL (InterMinDepthSTC,               6,   6,   3,   3,   2,   2,   2);
    TU_OPT_ALL (MotionPartitionDepth,           6,   6,   3,   1,   1,   1,   1);

    TU_OPT_ALL (AnalyzeCmplx,                   0,   0,   0,   0,   0,   0,   0);
    TU_OPT_ALL (RateControlDepth,               0,   0,   0,   0,   0,   0,   0);
    TU_OPT_ALL (LowresFactor,                   3,   3,   3,   3,   3,   3,   3);

    TU_OPT_ALL (RepackProb,                     0,   0,   0,   0,   0,   0,   0);

    TU_OPT_SW  (NumRefLayers,                   3,   3,   3,   3,   4,   4,   4);
    TU_OPT_GACC(NumRefLayers,                   2,   2,   3,   3,   4,   4,   4);


namespace H265Enc {

    const Ipp8u tab_defaultNumRefFrameSw[8]   = {2, 4, 4, 4, 4, 4, 4, 4};
    const Ipp8u tab_defaultNumRefFrameGacc[8] = {4, 4, 4, 4, 4, 4, 4, 4};

    const Ipp8u tab_defaultNumRefFrameLowDelaySw[8]   = {2, 4, 4, 4, 3, 3, 3, 2};
    const Ipp8u tab_defaultNumRefFrameLowDelayGacc[8] = {4, 4, 4, 4, 4, 4, 4, 3};

    const mfxExtCodingOptionHEVC tab_defaultOptHevcSw[8] = {
        TAB_TU(sw, 3), TAB_TU(sw, 0), TAB_TU(sw, 1), TAB_TU(sw, 2), TAB_TU(sw, 3), TAB_TU(sw, 4), TAB_TU(sw, 5), TAB_TU(sw, 6)
    };

    const mfxExtCodingOptionHEVC tab_defaultOptHevcGacc[8] = {
        TAB_TU(gacc, 3), TAB_TU(gacc, 0), TAB_TU(gacc, 1), TAB_TU(gacc, 2), TAB_TU(gacc, 3), TAB_TU(gacc, 4), TAB_TU(gacc, 5), TAB_TU(gacc, 6)
    };

#ifdef MFX_VA
    const Ipp8u DEFAULT_ENABLE_CM = ON;
#else
    const Ipp8u DEFAULT_ENABLE_CM = OFF;
#endif
};


#endif // MFX_ENABLE_H265_VIDEO_ENCODE2
