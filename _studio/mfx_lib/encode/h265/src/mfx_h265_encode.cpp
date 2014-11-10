//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfxdefs.h"
#include "mfx_common_int.h"
#include "mfx_task.h"
#include "mfx_brc_common.h"
#include "mfx_session.h"
#include "mfx_tools.h"
#include "vm_thread.h"
#include "vm_interlocked.h"
#include "vm_event.h"
#include "vm_sys_info.h"
#include "mfx_ext_buffers.h"
#include <new>
#include <numeric>
#include <math.h>

#include "mfx_h265_encode.h"

#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_frame.h"
#include "umc_structures.h"
#include "mfx_enc_common.h"

#if defined (MFX_VA)
#include "mfx_h265_enc_fei.h"
#endif // MFX_VA

using namespace H265Enc;

//////////////////////////////////////////////////////////////////////////

#define CHECK_OPTION(input, output, corcnt) \
    if ( input != MFX_CODINGOPTION_OFF &&     \
    input != MFX_CODINGOPTION_ON  &&      \
    input != MFX_CODINGOPTION_UNKNOWN ) { \
    output = MFX_CODINGOPTION_UNKNOWN;      \
    (corcnt) ++;                            \
} else output = input;

#define CHECK_EXTBUF_SIZE(ebuf, errcounter) if ((ebuf).Header.BufferSz != sizeof(ebuf)) {(errcounter) = (errcounter) + 1;}


namespace H265Enc {

    static const mfxU16 H265_MAXREFDIST = 16;//8;

    typedef struct
    {
        Ipp32s BufferSizeInKB;
        Ipp32s InitialDelayInKB;
        Ipp32s TargetKbps;
        Ipp32s MaxKbps;
    } RcParams;

    typedef struct
    {
        mfxEncodeCtrl *ctrl;
        mfxFrameSurface1 *surface;
        mfxBitstream *bs;

        // for ParallelFrames > 1
        Ipp32u m_taskID;
        volatile Ipp32u m_doStage;
        Task*           m_targetTask;      // this task (frame) has to be encoded completely. it is our expected output.
        volatile Ipp32u m_threadCount;
        volatile Ipp32u m_outputQueueSize; // to avoid mutex sync with list::size();
        volatile Ipp32u m_reencode;        // BRC repack

        // FEI
        volatile Ipp32u m_doStageFEI;

    } H265EncodeTaskInputParams;

    mfxExtBuffer HEVC_HEADER = { MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC) };

#define ON  MFX_CODINGOPTION_ON
#define OFF MFX_CODINGOPTION_OFF
#define UNK MFX_CODINGOPTION_UNKNOWN

#define TU_OPT_SW(opt, t1, t2, t3, t4, t5, t6, t7) \
    static const mfxU16 tab_sw_##opt[] = {t1, t2, t3, t4, t5, t6, t7}

#ifdef MFX_VA
#define TU_OPT_GACC(opt, t1, t2, t3, t4, t5, t6, t7) \
    static const mfxU16 tab_gacc_##opt[] = {t1, t2, t3, t4, t5, t6, t7};
#else //MFX_VA
#define TU_OPT_GACC(opt, t1, t2, t3, t4, t5, t6, t7)
#endif //MFX_VA

#define TU_OPT_ALL(opt, t1, t2, t3, t4, t5, t6, t7) \
    TU_OPT_SW(opt, t1, t2, t3, t4, t5, t6, t7); \
    TU_OPT_GACC(opt, t1, t2, t3, t4, t5, t6, t7)

#define TAB_TU(mode, x) {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, \
    tab_##mode##_Log2MaxCUSize[x],\
    tab_##mode##_MaxCUDepth[x],\
    tab_##mode##_QuadtreeTULog2MaxSize[x],\
    tab_##mode##_QuadtreeTULog2MinSize[x],\
    tab_##mode##_QuadtreeTUMaxDepthIntra[x],\
    tab_##mode##_QuadtreeTUMaxDepthInter[x],\
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
    tab_##mode##_GPB[x],\
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
    tab_##mode##_NumTileCols[x],\
    tab_##mode##_NumTileRows[x],\
    }

    // Extended bit depth
    TU_OPT_SW  (Enable10bit,                  OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_GACC(Enable10bit,                  OFF, OFF, OFF, OFF, OFF, OFF, OFF);

    // Mode decision CU/TU 
    TU_OPT_ALL (QuadtreeTULog2MaxSize,          5,   5,   5,   5,   5,   5,   5);
    TU_OPT_ALL (QuadtreeTULog2MinSize,          2,   2,   2,   2,   2,   2,   2);
    TU_OPT_SW  (Log2MaxCUSize,                  6,   6,   5,   5,   5,   5,   5);
    TU_OPT_SW  (MaxCUDepth,                     4,   4,   3,   3,   3,   3,   3);
    TU_OPT_SW  (QuadtreeTUMaxDepthIntra,        4,   3,   2,   2,   2,   1,   1);
#ifdef AMT_SETTINGS
    TU_OPT_SW  (QuadtreeTUMaxDepthInter,        3,   3,   2,   2,   2,   1,   1);
#else
    TU_OPT_SW  (QuadtreeTUMaxDepthInter,        4,   3,   2,   2,   2,   1,   1);
#endif
    TU_OPT_GACC(Log2MaxCUSize,                  5,   5,   5,   5,   5,   5,   5);
    TU_OPT_GACC(MaxCUDepth,                     3,   3,   3,   3,   3,   3,   3);
    TU_OPT_GACC(QuadtreeTUMaxDepthIntra,        3,   3,   2,   1,   1,   1,   1);
    TU_OPT_GACC(QuadtreeTUMaxDepthInter,        3,   3,   2,   1,   1,   1,   1);
    TU_OPT_ALL (TUSplitIntra,                   1,   1,   3,   3,   3,   3,   3);
    TU_OPT_ALL (CUSplit,                        2,   2,   2,   2,   2,   2,   2);
    TU_OPT_ALL (PuDecisionSatd,               OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (MinCUDepthAdapt,              OFF, OFF, OFF,  ON,  ON,  ON,  ON);
    TU_OPT_ALL (MaxCUDepthAdapt,               ON,  ON,  ON,  ON,  ON,  ON, OFF);
#ifdef AMT_THRESHOLDS
    TU_OPT_SW  (CUSplitThreshold,               0,   0,   0,  64, 192, 224, 256);
#else
    TU_OPT_SW  (CUSplitThreshold,               0,  64, 128, 192, 192, 224, 256);
#endif
    TU_OPT_GACC(CUSplitThreshold,               0,  64, 128, 192, 224, 224, 240);
    TU_OPT_SW  (PartModes,                      3,   2,   2,   1,   1,   1,   1);
    TU_OPT_GACC(PartModes,                      1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (FastSkip,                     OFF, OFF, OFF, OFF, OFF,  ON,  ON);
    TU_OPT_GACC(FastSkip,                     OFF, OFF, OFF,  ON,  ON,  ON,  ON);
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
    TU_OPT_GACC(SplitThresholdStrengthCUIntra,  1,   1,   1,   2,   3,   3,   3);
    TU_OPT_GACC(SplitThresholdStrengthTUIntra,  1,   1,   1,   2,   3,   3,   3);
    TU_OPT_GACC(SplitThresholdStrengthCUInter,  1,   1,   1,   2,   3,   3,   3);
    TU_OPT_ALL (SplitThresholdTabIndex       ,  1,   1,   1,   2,   2,   2,   2); //Tab1 + strength 3 - fastest combination

    //Chroma analysis
    TU_OPT_ALL (AnalyzeChroma,                 ON,  ON,  ON,  ON,  ON,  ON,  ON);
    TU_OPT_SW  (CostChroma,                    ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_GACC(CostChroma,                    ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_ALL (IntraChromaRDO,               OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (reserved,                       0,   0,   0,   0,   0,   0,   0);

    //Filtering
    TU_OPT_SW  (SAO,                           ON,  ON,  ON,  ON,  ON,  ON, OFF);
    TU_OPT_GACC(SAO,                           ON,  ON,  ON,  ON,  ON, OFF, OFF);
    TU_OPT_ALL (SaoOpt,                         1,   1,   2,   2,   2,   2,   2);
    TU_OPT_ALL (Deblocking,                    ON,  ON,  ON,  ON,  ON,  ON,  ON);

    //Intra prediction optimization
    TU_OPT_SW  (IntraAngModes,                  1,   1,   1,   1,   1,   1,   1); //I slice SW
    TU_OPT_GACC(IntraAngModes,                  1,   1,   1,   1,   1,   1,   1); //I slice Gacc
    TU_OPT_SW  (IntraAngModesP,                 1,   1,   2,   2,   2,   2,   3); //P slice SW
    TU_OPT_GACC(IntraAngModesP,                 2,   2,   2,   2,   2,   2,   3); //P slice Gacc
    TU_OPT_SW  (IntraAngModesBRef,              1,   1,   2,   2,   3,   3, 100); //B Ref slice SW
    TU_OPT_GACC(IntraAngModesBRef,              3,   3,   3,   3,   3,   3, 100); //B Ref slice Gacc
    TU_OPT_SW  (IntraAngModesBnonRef,           1,   1,   2,  99,  99, 100, 100); //B non Ref slice SW
    TU_OPT_GACC(IntraAngModesBnonRef,           3,   3,   3, 100, 100, 100, 100); //B non Ref slice Gacc

    //Quantization optimization
    TU_OPT_SW  (SignBitHiding,                 ON,  ON,  ON,  ON,  ON,  ON, OFF);
    TU_OPT_GACC(SignBitHiding,                 ON,  ON,  ON,  ON,  ON,  ON, OFF);
    TU_OPT_SW  (RDOQuant,                      ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_SW  (RDOQuantChroma,                ON,  ON,  ON, OFF, OFF, OFF, OFF);
    TU_OPT_SW  (RDOQuantCGZ,                   ON,  ON,  ON,  ON, OFF, OFF, OFF);
    TU_OPT_GACC(RDOQuant,                      OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_GACC(RDOQuantChroma,                OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_GACC(RDOQuantCGZ,                   OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (DeltaQpMode,                    0,   0,   0,   0,   0,   0,   0);

    //Intra RDO
    TU_OPT_SW  (IntraNumCand0_2,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand0_3,                2,   2,   2,   2,   2,   2,   2);
    TU_OPT_SW  (IntraNumCand0_4,                2,   2,   2,   2,   2,   2,   2);
    TU_OPT_SW  (IntraNumCand0_5,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand0_6,                1,   1,   1,   1,   1,   1,   1);
#ifdef AMT_SETTINGS
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
#else
    TU_OPT_SW  (IntraNumCand1_2,                8,   6,   4,   2,   2,   1,   1);
    TU_OPT_SW  (IntraNumCand1_3,                8,   6,   4,   2,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand1_4,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand1_5,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand1_6,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_2,                4,   3,   2,   2,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_3,                4,   3,   2,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_4,                2,   2,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_5,                2,   2,   1,   1,   1,   1,   1);
    TU_OPT_SW  (IntraNumCand2_6,                2,   2,   1,   1,   1,   1,   1);
#endif
    TU_OPT_GACC(IntraNumCand0_2,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand0_3,                2,   2,   2,   2,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand0_4,                2,   2,   2,   2,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand0_5,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand0_6,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand1_2,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand1_3,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand1_4,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand1_5,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand1_6,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_2,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_3,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_4,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_5,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(IntraNumCand2_6,                1,   1,   1,   1,   1,   1,   1);

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
    TU_OPT_SW  (HadamardMe,                     2,   2,   2,   2,   2,   1,   1);
    TU_OPT_GACC(HadamardMe,                     1,   1,   1,   1,   1,   1,   1);
    TU_OPT_ALL (PatternIntPel,                  1,   1,   1,   1,   1,   1,   1);
    TU_OPT_ALL (PatternSubPel,                  3,   3,   3,   3,   4,   4,   4); //4 -dia subpel search; 3- square (see enum SUBPEL_*)
    TU_OPT_ALL (NumBiRefineIter,              999, 999, 999,   3,   2,   1,   1); //999-practically infinite iteration
    TU_OPT_ALL (FastInterp,                   OFF, OFF, OFF, OFF, OFF, OFF, OFF); 
    TU_OPT_SW  (TryIntra,                     2,   2,   2,   2,   2,   2,   1);
    TU_OPT_GACC(TryIntra,                     1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (FastAMPSkipME,                2,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(FastAMPSkipME,                1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (FastAMPRD,                    2,   1,   1,   1,   1,   1,   1);
    TU_OPT_GACC(FastAMPRD,                    1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (SkipMotionPartition,          2,   2,   2,   1,   1,   1,   1);
    TU_OPT_GACC(SkipMotionPartition,          1,   1,   1,   1,   1,   1,   1);
    TU_OPT_SW  (SkipCandRD,                  ON,  ON,  ON,  OFF,  OFF, OFF, OFF);
    TU_OPT_GACC(SkipCandRD,                  OFF, OFF, OFF, OFF,  OFF, OFF, OFF);
    TU_OPT_ALL (FramesInParallel,               1,   1,   1,   1,   1,   1,   1);
    //GOP structure, reference options
    TU_OPT_SW  (GPB,                           ON,  ON,  ON,  ON,  ON, OFF, OFF);
    TU_OPT_GACC(GPB,                          OFF, OFF, OFF, OFF, OFF, OFF, OFF);
    TU_OPT_ALL (BPyramid,                      ON,  ON,  ON,  ON,  ON,  ON,  ON);

    TU_OPT_ALL (NumTileCols,                   1,   1,   1,   1,   1,   1,   1);
    TU_OPT_ALL (NumTileRows,                   1,   1,   1,   1,   1,   1,   1);

    Ipp8u tab_tuGopRefDist [8] = {8, 8, 8, 8, 8, 8, 8, 8};
#ifdef AMT_SETTINGS
    Ipp8u tab_tuNumRefFrame_SW[8] = {2, 4, 4, 3, 3, 2, 2, 2};
#else
    Ipp8u tab_tuNumRefFrame_SW[8] = {2, 4, 4, 4, 3, 3, 2, 2};
#endif
    Ipp8u tab_tuNumRefFrame_GACC[8] = {4, 4, 4, 4, 4, 4, 4, 2};


    mfxExtCodingOptionHEVC tab_tu[8] = {
        TAB_TU(sw, 3), TAB_TU(sw, 0), TAB_TU(sw, 1), TAB_TU(sw, 2), TAB_TU(sw, 3), TAB_TU(sw, 4), TAB_TU(sw, 5), TAB_TU(sw, 6)
    };

#ifdef MFX_VA
    mfxExtCodingOptionHEVC tab_tu_gacc[8] = {
        TAB_TU(gacc, 3), TAB_TU(gacc, 0), TAB_TU(gacc, 1), TAB_TU(gacc, 2), TAB_TU(gacc, 3), TAB_TU(gacc, 4), TAB_TU(gacc, 5), TAB_TU(gacc, 6)
    };
#endif //MFX_VA


    static const struct tab_hevcLevel {
        // General limits
        Ipp32s levelId;
        Ipp32s maxLumaPs;
        Ipp32s maxCPB[2]; // low/high tier, in 1000 bits
        Ipp32s maxSliceSegmentsPerPicture;
        Ipp32s maxTileRows;
        Ipp32s maxTileCols;
        // Main profiles limits
        Ipp64s maxLumaSr;
        Ipp32s maxBr[2]; // low/high tier, in 1000 bits
        Ipp32s minCr;
    } tab_level[] = {
        {MFX_LEVEL_HEVC_1 ,    36864, {   350,      0},  16,  1,  1,     552960, {   128,    128}, 2},
        {MFX_LEVEL_HEVC_2 ,   122880, {  1500,      0},  16,  1,  1,    3686400, {  1500,   1500}, 2},
        {MFX_LEVEL_HEVC_21,   245760, {  3000,      0},  20,  1,  1,    7372800, {  3000,   3000}, 2},
        {MFX_LEVEL_HEVC_3 ,   552960, {  6000,      0},  30,  2,  2,   16588800, {  6000,   6000}, 2},
        {MFX_LEVEL_HEVC_31,   983040, {  1000,      0},  40,  3,  3,   33177600, { 10000,  10000}, 2},
        {MFX_LEVEL_HEVC_4 ,  2228224, { 12000,  30000},  75,  5,  5,   66846720, { 12000,  30000}, 4},
        {MFX_LEVEL_HEVC_41,  2228224, { 20000,  50000},  75,  5,  5,  133693440, { 20000,  50000}, 4},
        {MFX_LEVEL_HEVC_5 ,  8912896, { 25000, 100000}, 200, 11, 10,  267386880, { 25000, 100000}, 6},
        {MFX_LEVEL_HEVC_51,  8912896, { 40000, 160000}, 200, 11, 10,  534773760, { 40000, 160000}, 8},
        {MFX_LEVEL_HEVC_52,  8912896, { 60000, 240000}, 200, 11, 10, 1069547520, { 60000, 240000}, 8},
        {MFX_LEVEL_HEVC_6 , 35651584, { 60000, 240000}, 600, 22, 20, 1069547520, { 60000, 240000}, 8},
        {MFX_LEVEL_HEVC_61, 35651584, {120000, 480000}, 600, 22, 20, 2139095040, {120000, 480000}, 8},
        {MFX_LEVEL_HEVC_62, 35651584, {240000, 800000}, 600, 22, 20, 4278190080, {240000, 800000}, 6}
    };

    static const int    NUM_265_LEVELS = sizeof(tab_level) / sizeof(tab_level[0]);

    //////////////////////////////////////////////////////////////////////////

    static mfxStatus CheckPlatformType(VideoCORE *core)
    {
#ifndef MFX_VA
        if (core && core->GetPlatformType() != MFX_PLATFORM_SOFTWARE)
            return MFX_WRN_PARTIAL_ACCELERATION;
#endif //!MFX_VA

        return MFX_ERR_NONE;
    }

    static Ipp32s Check265Level(Ipp32s inLevelTier, const mfxVideoParam *parMfx)
    {
        Ipp32s inLevel = inLevelTier &~ MFX_TIER_HEVC_HIGH;
        Ipp32s inTier = inLevelTier & MFX_TIER_HEVC_HIGH;
        Ipp32s level;
        for (level = 0; level < NUM_265_LEVELS; level++)
            if (tab_level[level].levelId == inLevel)
                break;

        if (level >= NUM_265_LEVELS) {
            if (inLevelTier != MFX_LEVEL_UNKNOWN)
                return MFX_LEVEL_UNKNOWN;
            inLevelTier = inLevel = MFX_LEVEL_UNKNOWN;
            inTier = 0;
            level = 0;
        }

        if (!parMfx) // just check for valid level value
            return inLevelTier;

        const mfxExtCodingOptionHEVC* opts  = (mfxExtCodingOptionHEVC*)GetExtBuffer(  parMfx->ExtParam,  parMfx->NumExtParam, MFX_EXTBUFF_HEVCENC );

        for( ; level < NUM_265_LEVELS; level++) {
            Ipp32s lumaPs = parMfx->mfx.FrameInfo.Width * parMfx->mfx.FrameInfo.Height;
            Ipp32s bitrate = parMfx->mfx.BRCParamMultiplier ? parMfx->mfx.BRCParamMultiplier * parMfx->mfx.TargetKbps : parMfx->mfx.TargetKbps;
            Ipp64s lumaSr = parMfx->mfx.FrameInfo.FrameRateExtD ? (Ipp64s)lumaPs * parMfx->mfx.FrameInfo.FrameRateExtN / parMfx->mfx.FrameInfo.FrameRateExtD : 0;
            if (lumaPs > tab_level[level].maxLumaPs)
                continue;
            if (parMfx->mfx.FrameInfo.Width * parMfx->mfx.FrameInfo.Width > 8 * tab_level[level].maxLumaPs)
                continue;
            if (parMfx->mfx.FrameInfo.Height * parMfx->mfx.FrameInfo.Height > 8 * tab_level[level].maxLumaPs)
                continue;
            if (parMfx->mfx.NumSlice > tab_level[level].maxSliceSegmentsPerPicture)
                continue;
            if (parMfx->mfx.FrameInfo.FrameRateExtD && lumaSr > tab_level[level].maxLumaSr)
                continue;
            if (10 * bitrate > 11 * tab_level[level].maxBr[1]) // try high tier, nal
                continue;
            if (bitrate && lumaSr * 3 / 2 * 8 / bitrate / 1000 < tab_level[level].minCr)
                continue; // it hardly can change the case

            // MaxDpbSIze not checked


            if (opts) {
                if (opts->Log2MaxCUSize > 0 && tab_level[level].levelId >= MFX_LEVEL_HEVC_5 && opts->Log2MaxCUSize < 5)
                    continue;
                if (opts->NumTileCols > tab_level[level].maxTileCols)
                    continue;
                if (opts->NumTileRows > tab_level[level].maxTileRows)
                    continue;
            }

            Ipp32s tier = (bitrate > tab_level[level].maxBr[0]) ? MFX_TIER_HEVC_HIGH : MFX_TIER_HEVC_MAIN;

            if (inLevel == tab_level[level].levelId && tier <= inTier)
                return inLevelTier;
            if (inLevel <= tab_level[level].levelId)
                return tab_level[level].levelId | tier;
        }
        return MFX_LEVEL_UNKNOWN;
    }

    void SetCalcParams( RcParams* rc, const mfxVideoParam *parMfx )
    {
        Ipp32s mult = IPP_MAX( parMfx->mfx.BRCParamMultiplier, 1);

        // not all fields are vlid for all rc modes
        rc->BufferSizeInKB = parMfx->mfx.BufferSizeInKB * mult;
        rc->TargetKbps = parMfx->mfx.TargetKbps * mult;
        rc->MaxKbps = parMfx->mfx.MaxKbps * mult;
        rc->InitialDelayInKB = parMfx->mfx.InitialDelayInKB * mult;
    }

    void GetCalcParams( mfxVideoParam *parMfx, const RcParams* rc, Ipp32s rcMode )
    {
        Ipp32s maxVal = rc->BufferSizeInKB;
        if (rcMode == MFX_RATECONTROL_AVBR)
            maxVal = IPP_MAX(rc->TargetKbps, maxVal);
        else if (rcMode != MFX_RATECONTROL_CQP)
            maxVal = IPP_MAX(rc->TargetKbps, IPP_MAX( IPP_MAX( rc->InitialDelayInKB, rc->MaxKbps), maxVal));

        Ipp32s mult = (Ipp32u)(maxVal + 0xffff) >> 16;
        if (mult == 0)
            mult = 1;

        parMfx->mfx.BRCParamMultiplier = (mfxU16)mult;
        parMfx->mfx.BufferSizeInKB = (mfxU16)(rc->BufferSizeInKB / mult);
        if (rcMode != MFX_RATECONTROL_CQP) {
            parMfx->mfx.TargetKbps = (mfxU16)(rc->TargetKbps / mult);
            if (rcMode != MFX_RATECONTROL_AVBR) {
                parMfx->mfx.MaxKbps = (mfxU16)(rc->MaxKbps / mult);
                parMfx->mfx.InitialDelayInKB = (mfxU16)(rc->InitialDelayInKB / mult);
            }
        }
    }


    //inline Ipp32u h265enc_ConvertBitrate(mfxU16 TargetKbps)
    //{
    //    return (TargetKbps * 1000);
    //}

    // check for known ExtBuffers, returns error code. or -1 if found unknown
    // zero mfxExtBuffer* are OK
    mfxStatus CheckExtBuffers_H265enc(mfxExtBuffer** ebuffers, mfxU32 nbuffers)
    {

        mfxU32 ID_list[] = { MFX_EXTBUFF_HEVCENC, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, MFX_EXTBUFF_DUMP };

        mfxU32 ID_found[sizeof(ID_list)/sizeof(ID_list[0])] = {0,};
        if (!ebuffers) return MFX_ERR_NONE;
        for(mfxU32 i=0; i<nbuffers; i++) {
            bool is_known = false;
            if (!ebuffers[i]) return MFX_ERR_NULL_PTR; //continue;
            for (mfxU32 j=0; j<sizeof(ID_list)/sizeof(ID_list[0]); j++)
                if (ebuffers[i]->BufferId == ID_list[j]) {
                    if (ID_found[j])
                        return MFX_ERR_UNDEFINED_BEHAVIOR;
                    is_known = true;
                    ID_found[j] = 1; // to avoid duplicated
                    break;
                }
                if (!is_known)
                    return MFX_ERR_UNSUPPORTED;
        }
        return MFX_ERR_NONE;
    }

} // namespace

//////////////////////////////////////////////////////////////////////////

MFXVideoENCODEH265::MFXVideoENCODEH265(VideoCORE *core, mfxStatus *stat)
    : VideoENCODE(),
    m_core(core),
    m_totalBits(0),
    m_encodedFrames(0),
    m_isInitialized(false),
    m_useSysOpaq(false),
    m_useVideoOpaq(false),
    m_isOpaque(false)
{
    m_brc = NULL;
    m_videoParam.m_logMvCostTable = NULL;
    ippStaticInit();
    *stat = MFX_ERR_NONE;
}

MFXVideoENCODEH265::~MFXVideoENCODEH265()
{
    Close();
}

mfxStatus MFXVideoENCODEH265::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(bs);
    MFX_CHECK_NULL_PTR1(bs->Data || !bs->MaxLength);

    H265ENC_UNREFERENCED_PARAMETER(pInternalParams);
    MFX_CHECK(bs->MaxLength >= (bs->DataOffset + bs->DataLength),MFX_ERR_UNDEFINED_BEHAVIOR);

    Ipp32s brcMult = IPP_MAX(1, m_mfxParam.mfx.BRCParamMultiplier);
    if ((Ipp32s)(bs->MaxLength - bs->DataOffset - bs->DataLength) < m_mfxParam.mfx.BufferSizeInKB * brcMult * 1000)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    if (surface) { // check frame parameters
        MFX_CHECK(surface->Info.ChromaFormat == m_mfxParam.mfx.FrameInfo.ChromaFormat, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Width  == m_mfxParam.mfx.FrameInfo.Width, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Height == m_mfxParam.mfx.FrameInfo.Height, MFX_ERR_INVALID_VIDEO_PARAM);

        if (surface->Data.Y) {
            MFX_CHECK (surface->Data.UV, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK (surface->Data.Pitch < 0x8000 && surface->Data.Pitch!=0, MFX_ERR_UNDEFINED_BEHAVIOR);
        } else {
            MFX_CHECK (!surface->Data.UV, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }

    *reordered_surface = surface;

    // why here???
    if (m_mfxParam.mfx.EncodedOrder) 
        return MFX_ERR_UNSUPPORTED;


    if (ctrl && (ctrl->FrameType != MFX_FRAMETYPE_UNKNOWN) && ((ctrl->FrameType & 0xff) != (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR))) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    m_frameCountSync++;

    mfxU32 noahead = 0;//1;
    Ipp32s lookaheadBuffering = m_videoParam.preEncMode > 0 ? 1 : 0;//2*m_mfxParam.mfx.GopRefDist + 1 : 0;// 9 in case of 3 B frames
    if (m_mfxHEVCOpts.EnableCm == MFX_CODINGOPTION_ON)
        noahead = 0;

    if (!surface) {
        if (m_frameCountBufferedSync == 0) { // buffered frames to be encoded
            return MFX_ERR_MORE_DATA;
        }
        m_frameCountBufferedSync --;
    }
    else if (m_frameCountSync > noahead && 
        (mfxI32)m_frameCountBufferedSync < m_mfxParam.mfx.GopRefDist + (m_videoParam.m_framesInParallel - 1) + (lookaheadBuffering) ) {

            m_frameCountBufferedSync++;
            return (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Init(mfxVideoParam* par_in)
{
    mfxStatus sts, stsQuery;
    MFX_CHECK(!m_isInitialized, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR1(par_in);
    MFX_CHECK(par_in->Protected == 0,MFX_ERR_INVALID_VIDEO_PARAM);

    sts = CheckVideoParamEncoders(par_in, m_core->IsExternalFrameAllocator(), MFX_HW_UNKNOWN);
    MFX_CHECK_STS(sts);

    sts = CheckExtBuffers_H265enc( par_in->ExtParam, par_in->NumExtParam );
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOptionHEVC* opts_hevc = (mfxExtCodingOptionHEVC*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_HEVCENC );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );
    mfxExtDumpFiles* opts_Dump = (mfxExtDumpFiles*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_DUMP );

    const mfxVideoParam *par = par_in;

    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtBuffer *ptr_checked_ext[3] = {0};
    mfxU16 ext_counter = 0;
    memset(&m_mfxParam,0,sizeof(mfxVideoParam));
    memset(&m_mfxHEVCOpts,0,sizeof(m_mfxHEVCOpts));
    memset(&m_mfxDumpFiles,0,sizeof(m_mfxDumpFiles));

    if (opts_hevc) {
        m_mfxHEVCOpts.Header.BufferId = MFX_EXTBUFF_HEVCENC;
        m_mfxHEVCOpts.Header.BufferSz = sizeof(m_mfxHEVCOpts);
        ptr_checked_ext[ext_counter++] = &m_mfxHEVCOpts.Header;
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    if (opts_Dump) {
        m_mfxDumpFiles.Header.BufferId = MFX_EXTBUFF_DUMP;
        m_mfxDumpFiles.Header.BufferSz = sizeof(m_mfxDumpFiles);
        ptr_checked_ext[ext_counter++] = &m_mfxDumpFiles.Header;
    }
    m_mfxParam.ExtParam = ptr_checked_ext;
    m_mfxParam.NumExtParam = ext_counter;

    stsQuery = Query(NULL, par_in, &m_mfxParam); // [has to] copy all provided params

    // return status for Init differs in these cases
    if (stsQuery == MFX_ERR_UNSUPPORTED &&
        (  (par_in->mfx.EncodedOrder != m_mfxParam.mfx.EncodedOrder) ||
        (par_in->mfx.NumSlice > 0 && m_mfxParam.mfx.NumSlice == 0) ||
        (par_in->IOPattern != 0 && m_mfxParam.IOPattern == 0) ||
        (par_in->mfx.RateControlMethod != 0 && m_mfxParam.mfx.RateControlMethod == 0) ||
        (par_in->mfx.FrameInfo.PicStruct != 0 && m_mfxParam.mfx.FrameInfo.PicStruct == 0) ||
        (par_in->mfx.FrameInfo.FrameRateExtN != 0 && m_mfxParam.mfx.FrameInfo.FrameRateExtN == 0) ||
        (par_in->mfx.FrameInfo.FrameRateExtD != 0 && m_mfxParam.mfx.FrameInfo.FrameRateExtD == 0) ||
        (par_in->mfx.FrameInfo.FourCC != 0 && m_mfxParam.mfx.FrameInfo.FourCC == 0) ||
        (par_in->mfx.FrameInfo.ChromaFormat != 0 && m_mfxParam.mfx.FrameInfo.ChromaFormat == 0) ) )
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (stsQuery != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        MFX_CHECK_STS(stsQuery);


    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (par->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        if (opaqAllocReq == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else {
            // check memory type in opaque allocation request
            //             if (!(opaqAllocReq->In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET) && !(opaqAllocReq->In.Type  & MFX_MEMTYPE_SYSTEM_MEMORY))
            //                 return MFX_ERR_INVALID_VIDEO_PARAM;

            //             if ((opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (opaqAllocReq->In.Type  & MFX_MEMTYPE_DXVA2_DECODER_TARGET))
            //                 return MFX_ERR_INVALID_VIDEO_PARAM;

            // use opaque surfaces. Need to allocate
            m_isOpaque = true;
        }
    }

    // return an error if requested opaque memory type isn't equal to native
    //     if (m_isOpaque && (opaqAllocReq->In.Type & MFX_MEMTYPE_FROM_ENCODE) && !(opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
    //         return MFX_ERR_INVALID_VIDEO_PARAM;

    //// to check if needed in hevc
    //m_allocator = new mfx_UMC_MemAllocator;
    //if (!m_allocator) return MFX_ERR_MEMORY_ALLOC;
    //
    //memset(&pParams, 0, sizeof(UMC::MemoryAllocatorParams));
    //m_allocator->InitMem(&pParams, m_core);

    if (m_isOpaque && opaqAllocReq->In.NumSurface < m_mfxParam.mfx.GopRefDist)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // Allocate Opaque frames and frame for copy from video memory (if any)
    memset(&m_auxInput, 0, sizeof(m_auxInput));
    m_useAuxInput = false;
    m_useSysOpaq = false;
    m_useVideoOpaq = false;
    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_isOpaque) {
        bool bOpaqVideoMem = m_isOpaque && !(opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY);
        bool bNeedAuxInput = (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) || bOpaqVideoMem;
        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        request.Info              = par->mfx.FrameInfo;

        // try to allocate opaque surfaces in video memory for another component in transcoding chain
        if (bOpaqVideoMem) {
            memset(&m_responseAlien, 0, sizeof(m_responseAlien));
            request.Type =  (mfxU16)opaqAllocReq->In.Type;
            request.NumFrameMin =request.NumFrameSuggested = (mfxU16)opaqAllocReq->In.NumSurface;

            sts = m_core->AllocFrames(&request,
                &m_responseAlien,
                opaqAllocReq->In.Surfaces,
                opaqAllocReq->In.NumSurface);

            if (MFX_ERR_NONE != sts &&
                MFX_ERR_UNSUPPORTED != sts) // unsupported means that current Core couldn;t allocate the surfaces
                return sts;

            if (m_responseAlien.NumFrameActual < request.NumFrameMin)
                return MFX_ERR_MEMORY_ALLOC;

            if (sts != MFX_ERR_UNSUPPORTED)
                m_useVideoOpaq = true;
        }

        // allocate all we need in system memory
        memset(&m_response, 0, sizeof(m_response));
        if (bNeedAuxInput) {
            // allocate additional surface in system memory for FastCopy from video memory
            request.Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
            request.NumFrameMin       = 1;
            request.NumFrameSuggested = 1;
            sts = m_core->AllocFrames(&request, &m_response);
            MFX_CHECK_STS(sts);
        } else {
            // allocate opaque surfaces in system memory
            request.Type =  (mfxU16)opaqAllocReq->In.Type;
            request.NumFrameMin       = opaqAllocReq->In.NumSurface;
            request.NumFrameSuggested = opaqAllocReq->In.NumSurface;
            sts = m_core->AllocFrames(&request,
                &m_response,
                opaqAllocReq->In.Surfaces,
                opaqAllocReq->In.NumSurface);
            MFX_CHECK_STS(sts);
        }

        if (m_response.NumFrameActual < request.NumFrameMin)
            return MFX_ERR_MEMORY_ALLOC;

        if (bNeedAuxInput) {
            m_useAuxInput = true;
            m_auxInput.Data.MemId = m_response.mids[0];
            m_auxInput.Info = request.Info;
        } else
            m_useSysOpaq = true;
    }


    mfxExtCodingOptionHEVC* opts_tu = &tab_tu[m_mfxParam.mfx.TargetUsage];
#ifdef MFX_VA
    if (m_mfxHEVCOpts.EnableCm != MFX_CODINGOPTION_OFF)
        opts_tu = &tab_tu_gacc[m_mfxParam.mfx.TargetUsage];
#endif // MFX_VA

    // check if size is aligned with CU depth
    int maxCUsize = m_mfxHEVCOpts.Log2MaxCUSize ? m_mfxHEVCOpts.Log2MaxCUSize : opts_tu->Log2MaxCUSize;
    int maxCUdepth = m_mfxHEVCOpts.MaxCUDepth ? m_mfxHEVCOpts.MaxCUDepth : opts_tu->MaxCUDepth;
    int minTUsize = m_mfxHEVCOpts.QuadtreeTULog2MinSize ? m_mfxHEVCOpts.QuadtreeTULog2MinSize : opts_tu->QuadtreeTULog2MinSize;

    int MinCUSize = 1 << (maxCUsize - maxCUdepth + 1);
    int tail = (m_mfxParam.mfx.FrameInfo.Width | m_mfxParam.mfx.FrameInfo.Height) & (MinCUSize-1);
    if (tail) { // size not aligned to minCU
        int logtail;
        for(logtail = 0; !(tail & (1<<logtail)); logtail++) ;
        if (logtail < 3) // min CU size is 8
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if (logtail < minTUsize) { // size aligned to TU size
            if (m_mfxHEVCOpts.QuadtreeTULog2MinSize)
                return MFX_ERR_INVALID_VIDEO_PARAM;
            minTUsize = m_mfxHEVCOpts.QuadtreeTULog2MinSize = (mfxU16)logtail;
        }
        if (m_mfxHEVCOpts.Log2MaxCUSize && m_mfxHEVCOpts.MaxCUDepth)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if (!m_mfxHEVCOpts.MaxCUDepth)
            maxCUdepth = m_mfxHEVCOpts.MaxCUDepth = (mfxU16)(maxCUsize + 1 - logtail);
        else // !m_mfxHEVCOpts.Log2MaxCUSize
            maxCUsize = m_mfxHEVCOpts.Log2MaxCUSize = (mfxU16)(maxCUdepth - 1 + logtail);
    }

    // then fill not provided params with defaults or from targret usage
    if (!opts_hevc) {
        m_mfxHEVCOpts = *opts_tu;
        m_mfxHEVCOpts.QuadtreeTULog2MinSize = (mfxU16)minTUsize;
        m_mfxHEVCOpts.MaxCUDepth = (mfxU16)maxCUdepth;
        m_mfxHEVCOpts.Log2MaxCUSize = (mfxU16)maxCUsize;
    } else { // complete unspecified params

        if(!m_mfxHEVCOpts.Log2MaxCUSize)
            m_mfxHEVCOpts.Log2MaxCUSize = opts_tu->Log2MaxCUSize;

        if(!m_mfxHEVCOpts.MaxCUDepth) // opts_in->MaxCUDepth <= opts_out->Log2MaxCUSize - 1
            m_mfxHEVCOpts.MaxCUDepth = IPP_MIN(opts_tu->MaxCUDepth, m_mfxHEVCOpts.Log2MaxCUSize - 1);

        if(!m_mfxHEVCOpts.QuadtreeTULog2MaxSize) // opts_in->QuadtreeTULog2MaxSize <= opts_out->Log2MaxCUSize
            m_mfxHEVCOpts.QuadtreeTULog2MaxSize = IPP_MIN(opts_tu->QuadtreeTULog2MaxSize, m_mfxHEVCOpts.Log2MaxCUSize);

        if(!m_mfxHEVCOpts.QuadtreeTULog2MinSize) // opts_in->QuadtreeTULog2MinSize <= opts_out->QuadtreeTULog2MaxSize
            m_mfxHEVCOpts.QuadtreeTULog2MinSize = IPP_MIN(opts_tu->QuadtreeTULog2MinSize, m_mfxHEVCOpts.QuadtreeTULog2MaxSize);

        if(!m_mfxHEVCOpts.QuadtreeTUMaxDepthIntra) // opts_in->QuadtreeTUMaxDepthIntra > opts_out->Log2MaxCUSize - 1
            m_mfxHEVCOpts.QuadtreeTUMaxDepthIntra = IPP_MIN(opts_tu->QuadtreeTUMaxDepthIntra, m_mfxHEVCOpts.Log2MaxCUSize - 1);

        if(!m_mfxHEVCOpts.QuadtreeTUMaxDepthInter) // opts_in->QuadtreeTUMaxDepthIntra > opts_out->Log2MaxCUSize - 1
            m_mfxHEVCOpts.QuadtreeTUMaxDepthInter = IPP_MIN(opts_tu->QuadtreeTUMaxDepthInter, m_mfxHEVCOpts.Log2MaxCUSize - 1);

        if (m_mfxHEVCOpts.AnalyzeChroma == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.AnalyzeChroma = opts_tu->AnalyzeChroma;
        if (m_mfxHEVCOpts.CostChroma == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.CostChroma = opts_tu->CostChroma;
        if (m_mfxHEVCOpts.IntraChromaRDO == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.IntraChromaRDO = opts_tu->IntraChromaRDO;
        if (m_mfxHEVCOpts.FastInterp == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.FastInterp = opts_tu->FastInterp;

        if (m_mfxHEVCOpts.RDOQuant == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.RDOQuant = opts_tu->RDOQuant;
        if (m_mfxHEVCOpts.SignBitHiding == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.SignBitHiding = opts_tu->SignBitHiding;

        if (m_mfxHEVCOpts.WPP == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.WPP = opts_tu->WPP;

        if (m_mfxHEVCOpts.GPB == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.GPB = opts_tu->GPB;

        if (m_mfxHEVCOpts.PartModes == 0)
            m_mfxHEVCOpts.PartModes = opts_tu->PartModes;

        if (m_mfxHEVCOpts.SAO == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.SAO = opts_tu->SAO;

        if (m_mfxHEVCOpts.BPyramid == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.BPyramid = opts_tu->BPyramid;

        if (!m_mfxHEVCOpts.SplitThresholdStrengthCUIntra)
            m_mfxHEVCOpts.SplitThresholdStrengthCUIntra = opts_tu->SplitThresholdStrengthCUIntra;
        if (!m_mfxHEVCOpts.SplitThresholdStrengthTUIntra)
            m_mfxHEVCOpts.SplitThresholdStrengthTUIntra = opts_tu->SplitThresholdStrengthTUIntra;
        if (!m_mfxHEVCOpts.SplitThresholdStrengthCUInter)
            m_mfxHEVCOpts.SplitThresholdStrengthCUInter = opts_tu->SplitThresholdStrengthCUInter;
        if (!m_mfxHEVCOpts.SplitThresholdTabIndex)
            m_mfxHEVCOpts.SplitThresholdTabIndex = opts_tu->SplitThresholdTabIndex;

        if (m_mfxHEVCOpts.IntraNumCand0_2 == 0)
            m_mfxHEVCOpts.IntraNumCand0_2 = opts_tu->IntraNumCand0_2;
        if (m_mfxHEVCOpts.IntraNumCand0_3 == 0)
            m_mfxHEVCOpts.IntraNumCand0_3 = opts_tu->IntraNumCand0_3;
        if (m_mfxHEVCOpts.IntraNumCand0_4 == 0)
            m_mfxHEVCOpts.IntraNumCand0_4 = opts_tu->IntraNumCand0_4;
        if (m_mfxHEVCOpts.IntraNumCand0_5 == 0)
            m_mfxHEVCOpts.IntraNumCand0_5 = opts_tu->IntraNumCand0_5;
        if (m_mfxHEVCOpts.IntraNumCand0_6 == 0)
            m_mfxHEVCOpts.IntraNumCand0_6 = opts_tu->IntraNumCand0_6;

        // take from tu, but correct if contradict with provided neighbours
        int pos;
        mfxU16 *candopt[10], candtu[10], *pleft[10], *plast;
        candopt[0] = &m_mfxHEVCOpts.IntraNumCand1_2;  candtu[0] = opts_tu->IntraNumCand1_2;
        candopt[1] = &m_mfxHEVCOpts.IntraNumCand1_3;  candtu[1] = opts_tu->IntraNumCand1_3;
        candopt[2] = &m_mfxHEVCOpts.IntraNumCand1_4;  candtu[2] = opts_tu->IntraNumCand1_4;
        candopt[3] = &m_mfxHEVCOpts.IntraNumCand1_5;  candtu[3] = opts_tu->IntraNumCand1_5;
        candopt[4] = &m_mfxHEVCOpts.IntraNumCand1_6;  candtu[4] = opts_tu->IntraNumCand1_6;
        candopt[5] = &m_mfxHEVCOpts.IntraNumCand2_2;  candtu[5] = opts_tu->IntraNumCand2_2;
        candopt[6] = &m_mfxHEVCOpts.IntraNumCand2_3;  candtu[6] = opts_tu->IntraNumCand2_3;
        candopt[7] = &m_mfxHEVCOpts.IntraNumCand2_4;  candtu[7] = opts_tu->IntraNumCand2_4;
        candopt[8] = &m_mfxHEVCOpts.IntraNumCand2_5;  candtu[8] = opts_tu->IntraNumCand2_5;
        candopt[9] = &m_mfxHEVCOpts.IntraNumCand2_6;  candtu[9] = opts_tu->IntraNumCand2_6;

        for(pos=0, plast=0; pos<10; pos++) {
            if (*candopt[pos]) plast = candopt[pos];
            pleft[pos] = plast;
        }
        for(pos=9, plast=0; pos>=0; pos--) {
            if (*candopt[pos]) plast = candopt[pos];
            else {
                mfxU16 val = candtu[pos];
                if(pleft[pos] && val>*pleft[pos]) val = *pleft[pos];
                if(plast      && val<*plast     ) val = *plast;
                *candopt[pos] = val;
            }
        }
        if (m_mfxHEVCOpts.TryIntra == 0)
            m_mfxHEVCOpts.TryIntra = opts_tu->TryIntra;
        if (m_mfxHEVCOpts.FastAMPSkipME == 0)
            m_mfxHEVCOpts.FastAMPSkipME = opts_tu->FastAMPSkipME;
        if (m_mfxHEVCOpts.FastAMPRD == 0)
            m_mfxHEVCOpts.FastAMPRD = opts_tu->FastAMPRD;
        if (m_mfxHEVCOpts.SkipMotionPartition == 0)
            m_mfxHEVCOpts.SkipMotionPartition = opts_tu->SkipMotionPartition;
        if (m_mfxHEVCOpts.SkipCandRD == 0)
            m_mfxHEVCOpts.SkipCandRD = opts_tu->SkipCandRD;
        if (m_mfxHEVCOpts.CmIntraThreshold == 0)
            m_mfxHEVCOpts.CmIntraThreshold = opts_tu->CmIntraThreshold;
        if (m_mfxHEVCOpts.TUSplitIntra == 0)
            m_mfxHEVCOpts.TUSplitIntra = opts_tu->TUSplitIntra;
        if (m_mfxHEVCOpts.CUSplit == 0)
            m_mfxHEVCOpts.CUSplit = opts_tu->CUSplit;
        if (m_mfxHEVCOpts.IntraAngModes == 0)
            m_mfxHEVCOpts.IntraAngModes = opts_tu->IntraAngModes;
        if (INTRA_ANG_MODE_DISABLE == m_mfxHEVCOpts.IntraAngModes) // Disable Intra in I frame is prohibited
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (m_mfxHEVCOpts.IntraAngModesP == 0)
            m_mfxHEVCOpts.IntraAngModesP = opts_tu->IntraAngModesP;
        if (m_mfxHEVCOpts.IntraAngModesBRef == 0)
            m_mfxHEVCOpts.IntraAngModesBRef = opts_tu->IntraAngModesBRef;
        if (m_mfxHEVCOpts.IntraAngModesBnonRef == 0)
            m_mfxHEVCOpts.IntraAngModesBnonRef = opts_tu->IntraAngModesBnonRef;
        if (m_mfxHEVCOpts.HadamardMe == 0)
            m_mfxHEVCOpts.HadamardMe = opts_tu->HadamardMe;
        if (m_mfxHEVCOpts.TMVP == 0)
            m_mfxHEVCOpts.TMVP = opts_tu->TMVP;
        if (m_mfxHEVCOpts.EnableCm == 0)
            m_mfxHEVCOpts.EnableCm = opts_tu->EnableCm;
        if (m_mfxHEVCOpts.Deblocking == 0)
            m_mfxHEVCOpts.Deblocking = opts_tu->Deblocking;
        if (m_mfxHEVCOpts.RDOQuantChroma == 0)
            m_mfxHEVCOpts.RDOQuantChroma = opts_tu->RDOQuantChroma;
        if (m_mfxHEVCOpts.RDOQuantCGZ == 0)
            m_mfxHEVCOpts.RDOQuantCGZ = opts_tu->RDOQuantCGZ;
        if (m_mfxHEVCOpts.SaoOpt == 0)
            m_mfxHEVCOpts.SaoOpt = opts_tu->SaoOpt;
        if (m_mfxHEVCOpts.PatternIntPel == 0)
            m_mfxHEVCOpts.PatternIntPel = opts_tu->PatternIntPel;
        if (m_mfxHEVCOpts.PatternSubPel == 0)
            m_mfxHEVCOpts.PatternSubPel = opts_tu->PatternSubPel;
        if (m_mfxHEVCOpts.FastSkip == 0)
            m_mfxHEVCOpts.FastSkip = opts_tu->FastSkip;
        if (m_mfxHEVCOpts.FastCbfMode == 0)
            m_mfxHEVCOpts.FastCbfMode = opts_tu->FastCbfMode;
        if (m_mfxHEVCOpts.PuDecisionSatd == 0)
            m_mfxHEVCOpts.PuDecisionSatd = opts_tu->PuDecisionSatd;
        if (m_mfxHEVCOpts.MinCUDepthAdapt == 0)
            m_mfxHEVCOpts.MinCUDepthAdapt = opts_tu->MinCUDepthAdapt;
        if (m_mfxHEVCOpts.MaxCUDepthAdapt == 0)
            m_mfxHEVCOpts.MaxCUDepthAdapt = opts_tu->MaxCUDepthAdapt;
        if (m_mfxHEVCOpts.NumBiRefineIter == 0)
            m_mfxHEVCOpts.NumBiRefineIter = opts_tu->NumBiRefineIter;
        if (m_mfxHEVCOpts.CUSplitThreshold == 0)
            m_mfxHEVCOpts.CUSplitThreshold = opts_tu->CUSplitThreshold;
        if (m_mfxHEVCOpts.DeltaQpMode == 0)
            m_mfxHEVCOpts.DeltaQpMode = opts_tu->DeltaQpMode;
        if (m_mfxHEVCOpts.Enable10bit == 0)
            m_mfxHEVCOpts.Enable10bit = opts_tu->Enable10bit;
        if (m_mfxHEVCOpts.FramesInParallel == 0)
            m_mfxHEVCOpts.FramesInParallel = opts_tu->FramesInParallel;
        if (m_mfxHEVCOpts.NumTileCols == 0)
            m_mfxHEVCOpts.NumTileCols = opts_tu->NumTileCols;
        if (m_mfxHEVCOpts.NumTileRows == 0)
            m_mfxHEVCOpts.NumTileRows = opts_tu->NumTileRows;
    }

    // uncomment here if sign bit hiding doesn't work properly
    //m_mfxHEVCOpts.SignBitHiding = MFX_CODINGOPTION_OFF;

    // check FrameInfo

    //FourCC to check on encode frame
    if (!m_mfxParam.mfx.FrameInfo.FourCC)
        m_mfxParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12; // to return on GetVideoParam

    // to be provided:
    if (!m_mfxParam.mfx.FrameInfo.Width || !m_mfxParam.mfx.FrameInfo.Height ||
        !m_mfxParam.mfx.FrameInfo.FrameRateExtN || !m_mfxParam.mfx.FrameInfo.FrameRateExtD)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // Crops, AspectRatio - ignore

    if (!m_mfxParam.mfx.FrameInfo.PicStruct) m_mfxParam.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    // CodecId - only HEVC;  CodecProfile CodecLevel - in the end
    // NumThread - set inside, >1 only if WPP
    //    mfxU32 asyncDepth = par->AsyncDepth ? par->AsyncDepth : m_core->GetAutoAsyncDepth();
    //    m_mfxVideoParam.mfx.NumThread = (mfxU16)asyncDepth;
    //    if (MFX_PLATFORM_SOFTWARE != MFX_Utility::GetPlatform(m_core, par_in))
    //        m_mfxVideoParam.mfx.NumThread = 1;
    m_mfxParam.mfx.NumThread = (mfxU16)vm_sys_info_get_cpu_num();
    if (m_mfxHEVCOpts.ForceNumThread > 0)
        m_mfxParam.mfx.NumThread = m_mfxHEVCOpts.ForceNumThread;

    /*#if defined (AS_HEVCE_PLUGIN)
    m_mfxVideoParam.mfx.NumThread += 1;
    #endif*/

    // TargetUsage - nothing to do

    // can depend on target usage
    if (!m_mfxParam.mfx.GopRefDist) {
        m_mfxParam.mfx.GopRefDist = tab_tuGopRefDist[m_mfxParam.mfx.TargetUsage];
    }
    if (!m_mfxParam.mfx.GopPicSize) {
        m_mfxParam.mfx.GopPicSize = 60 * (mfxU16) (( m_mfxParam.mfx.FrameInfo.FrameRateExtN + m_mfxParam.mfx.FrameInfo.FrameRateExtD - 1 ) / m_mfxParam.mfx.FrameInfo.FrameRateExtD);
        if (m_mfxParam.mfx.GopRefDist>1)
            m_mfxParam.mfx.GopPicSize = (mfxU16) ((m_mfxParam.mfx.GopPicSize + m_mfxParam.mfx.GopRefDist - 1) / m_mfxParam.mfx.GopRefDist * m_mfxParam.mfx.GopRefDist);
    }
    //if (!m_mfxVideoParam.mfx.IdrInterval) m_mfxVideoParam.mfx.IdrInterval = m_mfxVideoParam.mfx.GopPicSize * 8;
    // GopOptFlag ignore


    // to be provided:
    if (!m_mfxParam.mfx.RateControlMethod)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_mfxParam.mfx.BufferSizeInKB)
        if ((m_mfxParam.mfx.RateControlMethod == MFX_RATECONTROL_CBR || m_mfxParam.mfx.RateControlMethod == MFX_RATECONTROL_VBR) &&
            m_mfxParam.mfx.TargetKbps)
            m_mfxParam.mfx.BufferSizeInKB = m_mfxParam.mfx.TargetKbps * 2 / 8; // 2 seconds, like in AVC
        else
            m_mfxParam.mfx.BufferSizeInKB = par->mfx.FrameInfo.Width * par->mfx.FrameInfo.Height * 3 / 2000 + 1; // uncompressed

    if (!m_mfxParam.mfx.NumSlice) m_mfxParam.mfx.NumSlice = 1;
    if (m_mfxHEVCOpts.SAO == MFX_CODINGOPTION_ON && m_mfxParam.mfx.NumSlice > 1)
        m_mfxHEVCOpts.SAO = MFX_CODINGOPTION_OFF; // switch off SAO for now, because of inter-slice deblocking
    if (m_mfxParam.mfx.NumSlice > 1) {
        m_mfxHEVCOpts.NumTileCols = m_mfxHEVCOpts.NumTileRows = 1;
    }

    if (!m_mfxParam.mfx.NumRefFrame) {
        if (m_mfxParam.mfx.TargetUsage == MFX_TARGETUSAGE_1 && m_mfxParam.mfx.GopRefDist == 1) {
            m_mfxParam.mfx.NumRefFrame = 4; // Low_Delay
        } else {
            if(m_mfxHEVCOpts.EnableCm == MFX_CODINGOPTION_ON)
                m_mfxParam.mfx.NumRefFrame = tab_tuNumRefFrame_GACC[m_mfxParam.mfx.TargetUsage];
            else
                m_mfxParam.mfx.NumRefFrame = tab_tuNumRefFrame_SW[m_mfxParam.mfx.TargetUsage];
        }
    }

    // ALL INTRA
    if (m_mfxParam.mfx.GopPicSize == 1) {
        m_mfxParam.mfx.GopRefDist = 1;
        m_mfxParam.mfx.NumRefFrame = 1;
    }

    switch (m_mfxParam.mfx.FrameInfo.FourCC) {
    case MFX_FOURCC_P010:
        m_mfxParam.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
        break;
        //    case MFX_FOURCC_P210:
        //    case MFX_FOURCC_NV16:
        //        m_mfxVideoParam.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN422_10;
        //        break;
    case MFX_FOURCC_NV12:
    default:
        m_mfxParam.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
    }
    mfxI32 complevel = Check265Level(m_mfxParam.mfx.CodecLevel, &m_mfxParam);
    if (complevel != m_mfxParam.mfx.CodecLevel) {
        if (m_mfxParam.mfx.CodecLevel != MFX_LEVEL_UNKNOWN)
            stsQuery = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        m_mfxParam.mfx.CodecLevel = (mfxU16)complevel;
    }

    // what is still needed?
    //m_mfxVideoParam.mfx = par->mfx;
    //m_mfxVideoParam.SetCalcParams(&m_mfxVideoParam);
    //m_mfxVideoParam.calcParam = par->calcParam;
    m_mfxParam.IOPattern = par->IOPattern;
    m_mfxParam.Protected = 0;
    m_mfxParam.AsyncDepth = par->AsyncDepth;

    m_frameCountSync = 0;
    m_frameCount = 0;
    m_frameCountBufferedSync = 0;
    m_frameCountBuffered = 0;
    m_taskID = 0;

    sts = InitH265VideoParam(&m_mfxParam, &m_videoParam,  &m_mfxHEVCOpts);
    MFX_CHECK_STS(sts);

    sts = Init_Internal();
    MFX_CHECK_STS(sts);

    // frame threading model
    if (m_videoParam.m_framesInParallel > 1) {
        // criterion to start frame
        m_videoParam.m_lagBehindRefRows = 3;//IPP_MAX(Ipp32s(ratio), 3);
        m_videoParam.m_meSearchRangeY = (m_videoParam.m_lagBehindRefRows - 1) * m_videoParam.MaxCUSize; // -1 due to dblk lagging

        // new approach
        //m_videoParam.m_meSearchRangeY   = 128; // hardcoded for now!!!
        //m_videoParam.m_lagBehindRefRows = (m_videoParam.m_meSearchRangeY / m_videoParam.MaxCUSize ) + 1;
    }

    m_recon_dump_file_name = m_mfxDumpFiles.ReconFilename[0] ? m_mfxDumpFiles.ReconFilename : NULL;
    m_videoParam.reconForDump = m_recon_dump_file_name != NULL;

    m_frameEncoder.resize(m_videoParam.m_framesInParallel);
    for (size_t encIdx = 0; encIdx < m_frameEncoder.size(); encIdx++) {
        m_frameEncoder[encIdx] = new H265FrameEncoder();
        MFX_CHECK_STS_ALLOC(m_frameEncoder[encIdx]);

        sts =  m_frameEncoder[encIdx]->Init(&m_mfxParam, m_videoParam, &m_mfxHEVCOpts, m_core);
        MFX_CHECK_STS(sts);
    }
    // 

    m_isInitialized = true;

    m_semaphore.Init(0, INT_MAX);

    return stsQuery;
} // mfxStatus MFXVideoENCODEH265::Init(mfxVideoParam* par_in)


mfxStatus MFXVideoENCODEH265::Init_Internal( void )
{
    Ipp32u memSize = 0;

    memSize += (1 << 16);
    m_videoParam.m_logMvCostTable = (Ipp8u *)H265_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(m_videoParam.m_logMvCostTable);

    // init lookup table for 2*log2(x)+2
    m_videoParam.m_logMvCostTable[(1 << 15)] = 1;
    Ipp32f log2reciproc = 2 / log(2.0f);
    for (Ipp32s i = 1; i < (1 << 15); i++) {
        m_videoParam.m_logMvCostTable[(1 << 15) + i] = m_videoParam.m_logMvCostTable[(1 << 15) - i] = (Ipp8u)(log(i + 1.0f) * log2reciproc + 2);
    }

    m_profileIndex = 0;
    m_frameOrder = 0;

    m_frameOrderOfLastIdr = 0;       // frame order of last IDR frame (in display order)
    m_frameOrderOfLastIntra = 0;     // frame order of last I-frame (in display order)
    m_frameOrderOfLastAnchor = 0;    // frame order of last anchor (first in minigop) frame (in display order)
    m_miniGopCount = -1;
    m_lastTimeStamp = (mfxU64)MFX_TIMESTAMP_UNKNOWN;
    m_lastEncOrder = -1;

    //m_frameCountEncoded = 0;

    if (m_mfxParam.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        m_brc = new H265BRC();
        mfxStatus sts = m_brc->Init(&m_mfxParam, m_videoParam);
        MFX_CHECK_STS(sts);
    }

#if defined(MFX_ENABLE_H265_PAQ)
    if (m_videoParam.preEncMode) {

        int frameRate = m_videoParam.FrameRateExtN / m_videoParam.FrameRateExtD;
        int refDist =  m_videoParam.GopRefDist;
        int intraPeriod = m_videoParam.GopPicSize;
        int framesToBeEncoded = 610;//for test only

        m_preEnc.Init(NULL, m_videoParam.Width, m_videoParam.Height, 8, 8, frameRate, refDist, intraPeriod, framesToBeEncoded, m_videoParam.MaxCUSize);

        m_pAQP = NULL;
        m_pAQP = new TAdapQP;
        int maxCUWidth = m_videoParam.MaxCUSize;
        int maxCUHeight = m_videoParam.MaxCUSize;
        m_pAQP->Init(maxCUWidth, maxCUHeight, m_videoParam.Width, m_videoParam.Height, refDist);
    }
#endif

    memset(m_videoParam.cu_split_threshold_cu, 0, 52 * 2 * MAX_TOTAL_DEPTH * sizeof(Ipp64f));
    memset(m_videoParam.cu_split_threshold_tu, 0, 52 * 2 * MAX_TOTAL_DEPTH * sizeof(Ipp64f));

    Ipp32s qpMax = 42;
    for (Ipp32s QP = 10; QP <= qpMax; QP++) {
        for (Ipp32s isNotI = 0; isNotI <= 1; isNotI++) {
            for (Ipp32s i = 0; i < m_videoParam.MaxCUDepth; i++) {
                m_videoParam.cu_split_threshold_cu[QP][isNotI][i] = h265_calc_split_threshold(m_videoParam.SplitThresholdTabIndex, 0, isNotI, m_videoParam.Log2MaxCUSize - i,
                    isNotI ? m_videoParam.SplitThresholdStrengthCUInter : m_videoParam.SplitThresholdStrengthCUIntra, QP);
                m_videoParam.cu_split_threshold_tu[QP][isNotI][i] = h265_calc_split_threshold(m_videoParam.SplitThresholdTabIndex, 1, isNotI, m_videoParam.Log2MaxCUSize - i,
                    m_videoParam.SplitThresholdStrengthTUIntra, QP);
            }
        }
    }
    for (Ipp32s QP = qpMax + 1; QP <= 51; QP++) {
        for (Ipp32s isNotI = 0; isNotI <= 1; isNotI++) {
            for (Ipp32s i = 0; i < m_videoParam.MaxCUDepth; i++) {
                m_videoParam.cu_split_threshold_cu[QP][isNotI][i] = m_videoParam.cu_split_threshold_cu[qpMax][isNotI][i];
                m_videoParam.cu_split_threshold_tu[QP][isNotI][i] = m_videoParam.cu_split_threshold_tu[qpMax][isNotI][i];
            }
        }
    }

    SetProfileLevel();
    SetVPS();
    SetSPS();
    SetPPS();

    m_videoParam.m_profile_level = &m_profile_level;
    m_videoParam.m_vps = &m_vps;
    m_videoParam.csps = &m_sps;
    m_videoParam.cpps = &m_pps;

#if defined (MFX_VA)
    if (m_videoParam.enableCmFlag) {
        m_FeiCtx = new FeiContext(&m_videoParam, m_core);
    } else {
        m_FeiCtx = NULL;
    }
#endif // MFX_VA

    MFX_HEVC_PP::InitDispatcher(m_videoParam.cpuFeature);

#if defined(DUMP_COSTS_CU) || defined (DUMP_COSTS_TU)
    char fname[100];
#ifdef DUMP_COSTS_CU
    sprintf(fname, "thres_cu_%d.bin",param->mfx.TargetUsage);
    if (!(fp_cu = fopen(fname,"ab"))) return MFX_ERR_UNKNOWN;
#endif
#ifdef DUMP_COSTS_TU
    sprintf(fname, "thres_tu_%d.bin",param->mfx.TargetUsage);
    if (!(fp_tu = fopen(fname,"ab"))) return MFX_ERR_UNKNOWN;
#endif
#endif

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Close()
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    //-----------------------------------------------------
    // release resource of frame control
    // [1] frames
    for(FramePtrIter frm = m_freeFrames.begin(); frm != m_freeFrames.end(); frm++) {
        (*frm)->Destroy();
        delete (*frm);
    }
    m_freeFrames.resize(0);

    // [2] task
    for(TaskIter it = m_free.begin(); it != m_free.end(); it++) {
        delete (*it);
    }
    for(TaskIter it = m_inputQueue.begin(); it != m_inputQueue.end(); it++) {
        delete (*it);
    }
    for(TaskIter it = m_dpb.begin(); it != m_dpb.end(); it++) {
        delete (*it);
    }
    // note: m_lookaheadQueue() & m_encodeQueue() only "refer on tasks", not have real ptr. so we don't need to release ones
    m_free.resize(0);
    m_inputQueue.resize(0);
    m_dpb.resize(0);
    //-----------------------------------------------------

    if (m_brc) {
        delete m_brc;
        m_brc = NULL;
    }

    if (m_videoParam.m_logMvCostTable) {
        H265_Free(m_videoParam.m_logMvCostTable);
        m_videoParam.m_logMvCostTable = NULL;
    }


    if (!m_frameEncoder.empty()) {
        for(size_t encIdx = 0; encIdx < m_frameEncoder.size(); encIdx++) {
            H265FrameEncoder* fenc = m_frameEncoder[encIdx];
            if(fenc) {
                fenc->Close();
                delete fenc;
                m_frameEncoder[encIdx] = NULL;
            }
        }
    }

    if (m_useAuxInput || m_useSysOpaq) {
        //st = m_core->UnlockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
        m_core->FreeFrames(&m_response);
        m_response.NumFrameActual = 0;
        m_useAuxInput = false;
        m_useSysOpaq = false;
    }

    if (m_useVideoOpaq) {
        m_core->FreeFrames(&m_responseAlien);
        m_responseAlien.NumFrameActual = 0;
        m_useVideoOpaq = false;
    }

#if defined (MFX_VA)
    if (m_videoParam.enableCmFlag) {
        /*
        #if defined(_WIN32) || defined(_WIN64)
        PrintTimes();
        #endif // #if defined(_WIN32) || defined(_WIN64)
        //delete m_cmCtx;
        */
        if (m_FeiCtx)
            delete m_FeiCtx;

        m_FeiCtx = NULL;
    }
#endif // MFX_VA

#if defined(MFX_ENABLE_H265_PAQ)
    if (m_videoParam.preEncMode) {
        m_preEnc.Close();
        if(m_pAQP) {
            m_pAQP->Close();
            delete m_pAQP;
        }
    }
#endif

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Reset(mfxVideoParam *par_in)
{
    mfxStatus sts, stsQuery;

    MFX_CHECK_NULL_PTR1(par_in);
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    sts = CheckExtBuffers_H265enc( par_in->ExtParam, par_in->NumExtParam );
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOptionHEVC* opts_hevc = (mfxExtCodingOptionHEVC*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_HEVCENC );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );
    mfxExtDumpFiles* opts_Dump = (mfxExtDumpFiles*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_DUMP );

    mfxVideoParam parNew = *par_in;
    mfxVideoParam& parOld = m_mfxParam;

    //set unspecified parameters
    if (!parNew.AsyncDepth)             parNew.AsyncDepth           = parOld.AsyncDepth;
    //if (!parNew.IOPattern)              parNew.IOPattern            = parOld.IOPattern;
    if (!parNew.mfx.FrameInfo.FourCC)   parNew.mfx.FrameInfo.FourCC = parOld.mfx.FrameInfo.FourCC;
    if (!parNew.mfx.FrameInfo.FrameRateExtN) parNew.mfx.FrameInfo.FrameRateExtN = parOld.mfx.FrameInfo.FrameRateExtN;
    if (!parNew.mfx.FrameInfo.FrameRateExtD) parNew.mfx.FrameInfo.FrameRateExtD = parOld.mfx.FrameInfo.FrameRateExtD;
    if (!parNew.mfx.FrameInfo.AspectRatioW)  parNew.mfx.FrameInfo.AspectRatioW  = parOld.mfx.FrameInfo.AspectRatioW;
    if (!parNew.mfx.FrameInfo.AspectRatioH)  parNew.mfx.FrameInfo.AspectRatioH  = parOld.mfx.FrameInfo.AspectRatioH;
    if (!parNew.mfx.FrameInfo.PicStruct)     parNew.mfx.FrameInfo.PicStruct     = parOld.mfx.FrameInfo.PicStruct;
    //if (!parNew.mfx.FrameInfo.ChromaFormat) parNew.mfx.FrameInfo.ChromaFormat = parOld.mfx.FrameInfo.ChromaFormat;
    if (!parNew.mfx.CodecProfile)       parNew.mfx.CodecProfile      = parOld.mfx.CodecProfile;
    if (!parNew.mfx.CodecLevel)         parNew.mfx.CodecLevel        = parOld.mfx.CodecLevel;
    if (!parNew.mfx.NumThread)          parNew.mfx.NumThread         = parOld.mfx.NumThread;
    if (!parNew.mfx.TargetUsage)        parNew.mfx.TargetUsage       = parOld.mfx.TargetUsage;

    if (!parNew.mfx.FrameInfo.Width)    parNew.mfx.FrameInfo.Width  = parOld.mfx.FrameInfo.Width;
    if (!parNew.mfx.FrameInfo.Height)   parNew.mfx.FrameInfo.Height = parOld.mfx.FrameInfo.Height;
    //if (!parNew.mfx.FrameInfo.CropX)    parNew.mfx.FrameInfo.CropX  = parOld.mfx.FrameInfo.CropX;
    //if (!parNew.mfx.FrameInfo.CropY)    parNew.mfx.FrameInfo.CropY  = parOld.mfx.FrameInfo.CropY;
    //if (!parNew.mfx.FrameInfo.CropW)    parNew.mfx.FrameInfo.CropW  = parOld.mfx.FrameInfo.CropW;
    //if (!parNew.mfx.FrameInfo.CropH)    parNew.mfx.FrameInfo.CropH  = parOld.mfx.FrameInfo.CropH;
    if (!parNew.mfx.GopRefDist)         parNew.mfx.GopRefDist        = parOld.mfx.GopRefDist;
    if (!parNew.mfx.GopOptFlag)         parNew.mfx.GopOptFlag        = parOld.mfx.GopOptFlag;
    if (!parNew.mfx.RateControlMethod)  parNew.mfx.RateControlMethod = parOld.mfx.RateControlMethod;
    if (!parNew.mfx.NumSlice)           parNew.mfx.NumSlice          = parOld.mfx.NumSlice;
    if (!parNew.mfx.NumRefFrame)        parNew.mfx.NumRefFrame       = parOld.mfx.NumRefFrame;

    /*if(    parNew.mfx.CodecProfile != MFX_PROFILE_AVC_STEREO_HIGH
    && parNew.mfx.CodecProfile != MFX_PROFILE_AVC_MULTIVIEW_HIGH)*/{
        mfxU16 old_multiplier = IPP_MAX(parOld.mfx.BRCParamMultiplier, 1);
        mfxU16 new_multiplier = IPP_MAX(parNew.mfx.BRCParamMultiplier, 1);

        if (old_multiplier > new_multiplier) {
            parNew.mfx.BufferSizeInKB = (mfxU16)((mfxU64)parNew.mfx.BufferSizeInKB*new_multiplier/old_multiplier);
            if (parNew.mfx.RateControlMethod == MFX_RATECONTROL_CBR || parNew.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
                parNew.mfx.InitialDelayInKB = (mfxU16)((mfxU64)parNew.mfx.InitialDelayInKB*new_multiplier/old_multiplier);
                parNew.mfx.TargetKbps       = (mfxU16)((mfxU64)parNew.mfx.TargetKbps      *new_multiplier/old_multiplier);
                parNew.mfx.MaxKbps          = (mfxU16)((mfxU64)parNew.mfx.MaxKbps         *new_multiplier/old_multiplier);
            }
            new_multiplier = old_multiplier;
            parNew.mfx.BRCParamMultiplier = new_multiplier;
        }
        if (!parNew.mfx.BufferSizeInKB) parNew.mfx.BufferSizeInKB = (mfxU16)((mfxU64)parOld.mfx.BufferSizeInKB*old_multiplier/new_multiplier);
        if (parNew.mfx.RateControlMethod == parOld.mfx.RateControlMethod) {
            if (parNew.mfx.RateControlMethod > MFX_RATECONTROL_VBR)
                old_multiplier = new_multiplier = 1;
            if (!parNew.mfx.InitialDelayInKB) parNew.mfx.InitialDelayInKB = (mfxU16)((mfxU64)parOld.mfx.InitialDelayInKB*old_multiplier/new_multiplier);
            if (!parNew.mfx.TargetKbps)       parNew.mfx.TargetKbps       = (mfxU16)((mfxU64)parOld.mfx.TargetKbps      *old_multiplier/new_multiplier);
            if (!parNew.mfx.MaxKbps)          parNew.mfx.MaxKbps          = (mfxU16)((mfxU64)parOld.mfx.MaxKbps         *old_multiplier/new_multiplier);
        }
    }

    mfxExtCodingOptionHEVC optsNew;
    if (opts_hevc) {
        mfxExtCodingOptionHEVC& optsOld = m_mfxHEVCOpts;
        optsNew = *opts_hevc;
        if (!optsNew.Log2MaxCUSize                ) optsNew.Log2MaxCUSize                 = optsOld.Log2MaxCUSize;
        if (!optsNew.MaxCUDepth                   ) optsNew.MaxCUDepth                    = optsOld.MaxCUDepth;
        if (!optsNew.QuadtreeTULog2MaxSize        ) optsNew.QuadtreeTULog2MaxSize         = optsOld.QuadtreeTULog2MaxSize        ;
        if (!optsNew.QuadtreeTULog2MinSize        ) optsNew.QuadtreeTULog2MinSize         = optsOld.QuadtreeTULog2MinSize        ;
        if (!optsNew.QuadtreeTUMaxDepthIntra      ) optsNew.QuadtreeTUMaxDepthIntra       = optsOld.QuadtreeTUMaxDepthIntra      ;
        if (!optsNew.QuadtreeTUMaxDepthInter      ) optsNew.QuadtreeTUMaxDepthInter       = optsOld.QuadtreeTUMaxDepthInter      ;
        if (!optsNew.AnalyzeChroma                ) optsNew.AnalyzeChroma                 = optsOld.AnalyzeChroma                ;
        if (!optsNew.SignBitHiding                ) optsNew.SignBitHiding                 = optsOld.SignBitHiding                ;
        if (!optsNew.RDOQuant                     ) optsNew.RDOQuant                      = optsOld.RDOQuant                     ;
        if (!optsNew.SAO                          ) optsNew.SAO                           = optsOld.SAO                          ;
        if (!optsNew.SplitThresholdStrengthCUIntra) optsNew.SplitThresholdStrengthCUIntra = optsOld.SplitThresholdStrengthCUIntra;
        if (!optsNew.SplitThresholdStrengthTUIntra) optsNew.SplitThresholdStrengthTUIntra = optsOld.SplitThresholdStrengthTUIntra;
        if (!optsNew.SplitThresholdStrengthCUInter) optsNew.SplitThresholdStrengthCUInter = optsOld.SplitThresholdStrengthCUInter;
        if (!optsNew.SplitThresholdTabIndex       ) optsNew.SplitThresholdTabIndex        = optsOld.SplitThresholdTabIndex       ;
        if (!optsNew.IntraNumCand1_2              ) optsNew.IntraNumCand1_2               = optsOld.IntraNumCand1_2              ;
        if (!optsNew.IntraNumCand1_3              ) optsNew.IntraNumCand1_3               = optsOld.IntraNumCand1_3              ;
        if (!optsNew.IntraNumCand1_4              ) optsNew.IntraNumCand1_4               = optsOld.IntraNumCand1_4              ;
        if (!optsNew.IntraNumCand1_5              ) optsNew.IntraNumCand1_5               = optsOld.IntraNumCand1_5              ;
        if (!optsNew.IntraNumCand1_6              ) optsNew.IntraNumCand1_6               = optsOld.IntraNumCand1_6              ;
        if (!optsNew.IntraNumCand2_2              ) optsNew.IntraNumCand2_2               = optsOld.IntraNumCand2_2              ;
        if (!optsNew.IntraNumCand2_3              ) optsNew.IntraNumCand2_3               = optsOld.IntraNumCand2_3              ;
        if (!optsNew.IntraNumCand2_4              ) optsNew.IntraNumCand2_4               = optsOld.IntraNumCand2_4              ;
        if (!optsNew.IntraNumCand2_5              ) optsNew.IntraNumCand2_5               = optsOld.IntraNumCand2_5              ;
        if (!optsNew.IntraNumCand2_6              ) optsNew.IntraNumCand2_6               = optsOld.IntraNumCand2_6              ;
        if (!optsNew.WPP                          ) optsNew.WPP                           = optsOld.WPP                          ;
        if (!optsNew.GPB                          ) optsNew.GPB                           = optsOld.GPB                          ;
        if (!optsNew.PartModes                    ) optsNew.PartModes                     = optsOld.PartModes                    ;
        if (!optsNew.BPyramid                     ) optsNew.BPyramid                      = optsOld.BPyramid                     ;
        if (!optsNew.CostChroma                   ) optsNew.CostChroma                    = optsOld.CostChroma                   ;
        if (!optsNew.IntraChromaRDO               ) optsNew.IntraChromaRDO                = optsOld.IntraChromaRDO               ;
        if (!optsNew.FastInterp                   ) optsNew.FastInterp                    = optsOld.FastInterp                   ;
        if (!optsNew.FastSkip                     ) optsNew.FastSkip                      = optsOld.FastSkip                     ;
        if (!optsNew.FastCbfMode                  ) optsNew.FastCbfMode                   = optsOld.FastCbfMode                  ;
        if (!optsNew.PuDecisionSatd               ) optsNew.PuDecisionSatd                = optsOld.PuDecisionSatd               ;
        if (!optsNew.MinCUDepthAdapt              ) optsNew.MinCUDepthAdapt               = optsOld.MinCUDepthAdapt              ;
        if (!optsNew.MaxCUDepthAdapt              ) optsNew.MaxCUDepthAdapt               = optsOld.MaxCUDepthAdapt              ;
        if (!optsNew.CUSplitThreshold             ) optsNew.CUSplitThreshold              = optsOld.CUSplitThreshold             ;
        if (!optsNew.DeltaQpMode                  ) optsNew.DeltaQpMode                   = optsOld.DeltaQpMode                  ;
        if (!optsNew.Enable10bit                  ) optsNew.Enable10bit                   = optsOld.Enable10bit                  ;
        if (!optsNew.CpuFeature                   ) optsNew.CpuFeature                    = optsOld.CpuFeature                   ;
        if (!optsNew.TryIntra                     ) optsNew.TryIntra                      = optsOld.TryIntra                     ;
        if (!optsNew.FastAMPSkipME                ) optsNew.FastAMPSkipME                 = optsOld.FastAMPSkipME                ;
        if (!optsNew.FastAMPRD                    ) optsNew.FastAMPRD                     = optsOld.FastAMPRD                    ;
        if (!optsNew.SkipMotionPartition          ) optsNew.SkipMotionPartition           = optsOld.SkipMotionPartition          ;
        if (!optsNew.SkipCandRD                   ) optsNew.SkipCandRD                    = optsOld.SkipCandRD                   ;
        if (!optsNew.FramesInParallel             ) optsNew.FramesInParallel              = optsOld.FramesInParallel             ;
        if (!optsNew.NumTileCols                 ) optsNew.NumTileCols                  = optsOld.NumTileCols                 ;
        if (!optsNew.NumTileRows                 ) optsNew.NumTileRows                  = optsOld.NumTileRows                 ;
    }

    if ((parNew.IOPattern & 0xffc8) || (parNew.IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (parNew.IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;


    sts = CheckVideoParamEncoders(&parNew, m_core->IsExternalFrameAllocator(), MFX_HW_UNKNOWN);
    MFX_CHECK_STS(sts);

    mfxVideoParam checked_videoParam = parNew;
    mfxExtCodingOptionHEVC checked_codingOptionHEVC = m_mfxHEVCOpts;
    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtDumpFiles checked_DumpFiles = m_mfxDumpFiles;

    mfxExtBuffer *ptr_input_ext[3] = {0};
    mfxExtBuffer *ptr_checked_ext[3] = {0};
    mfxU16 ext_counter = 0;

    if (opts_hevc) {
        ptr_input_ext  [ext_counter  ] = &optsNew.Header;
        ptr_checked_ext[ext_counter++] = &checked_codingOptionHEVC.Header;
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_input_ext  [ext_counter  ] = &opaqAllocReq->Header;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    if (opts_Dump) {
        ptr_input_ext  [ext_counter  ] = &opts_Dump->Header;
        ptr_checked_ext[ext_counter++] = &checked_DumpFiles.Header;
    }
    parNew.ExtParam = ptr_input_ext;
    parNew.NumExtParam = ext_counter;
    checked_videoParam.ExtParam = ptr_checked_ext;
    checked_videoParam.NumExtParam = ext_counter;

    stsQuery = Query(NULL, &parNew, &checked_videoParam); // [has to] copy all provided params

    if (stsQuery != MFX_ERR_NONE && stsQuery != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) {
        if (stsQuery == MFX_ERR_UNSUPPORTED)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else
            return stsQuery;
    }

    // check that new params don't require allocation of additional memory
    if (checked_videoParam.mfx.FrameInfo.Width > m_mfxParam.mfx.FrameInfo.Width ||
        checked_videoParam.mfx.FrameInfo.Height > m_mfxParam.mfx.FrameInfo.Height ||
        checked_videoParam.mfx.GopRefDist <  m_mfxParam.mfx.GopRefDist ||
        checked_videoParam.mfx.NumSlice != m_mfxParam.mfx.NumSlice ||
        checked_videoParam.mfx.NumRefFrame != m_mfxParam.mfx.NumRefFrame ||
        checked_videoParam.AsyncDepth != m_mfxParam.AsyncDepth ||
        checked_videoParam.IOPattern != m_mfxParam.IOPattern ||
        (checked_videoParam.mfx.CodecProfile & 0xFF) != (m_mfxParam.mfx.CodecProfile & 0xFF)  )
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // Now use simple reset
    Close();
    m_isInitialized = false;
    sts = Init(&checked_videoParam);

    // Not finished implementation

    //m_frameCountSync = 0;
    //m_frameCount = 0;
    //m_frameCountBufferedSync = 0;
    //m_frameCountBuffered = 0;

    //    sts = m_enc->Reset();
    //    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus MFXVideoENCODEH265::Query(VideoCORE *core, mfxVideoParam *par_in, mfxVideoParam *par_out)
{
    mfxU32 isCorrected = 0;
    mfxU32 isInvalid = 0;
    mfxStatus st;
    MFX_CHECK_NULL_PTR1(par_out)

        //First check for zero input
        if( par_in == NULL ){ //Set ones for filed that can be configured
            mfxVideoParam *out = par_out;
            memset( &out->mfx, 0, sizeof( mfxInfoMFX ));
            out->mfx.FrameInfo.FourCC = 1;
            out->mfx.FrameInfo.Width = 1;
            out->mfx.FrameInfo.Height = 1;
            out->mfx.FrameInfo.CropX = 1;
            out->mfx.FrameInfo.CropY = 1;
            out->mfx.FrameInfo.CropW = 1;
            out->mfx.FrameInfo.CropH = 1;
            out->mfx.FrameInfo.FrameRateExtN = 1;
            out->mfx.FrameInfo.FrameRateExtD = 1;
            out->mfx.FrameInfo.AspectRatioW = 1;
            out->mfx.FrameInfo.AspectRatioH = 1;
            out->mfx.FrameInfo.PicStruct = 1;
            out->mfx.FrameInfo.ChromaFormat = 1;
            out->mfx.CodecId = MFX_CODEC_HEVC; // restore cleared mandatory
            out->mfx.CodecLevel = 1;
            out->mfx.CodecProfile = 1;
            out->mfx.NumThread = 1;
            out->mfx.TargetUsage = 1;
            out->mfx.GopPicSize = 1;
            out->mfx.GopRefDist = 1;
            out->mfx.GopOptFlag = 1;
            out->mfx.IdrInterval = 1;
            out->mfx.RateControlMethod = 1;
            out->mfx.InitialDelayInKB = 1;
            out->mfx.BufferSizeInKB = 1;
            out->mfx.TargetKbps = 1;
            out->mfx.MaxKbps = 1;
            out->mfx.NumSlice = 1;
            out->mfx.NumRefFrame = 1;
            out->mfx.EncodedOrder = 0;
            out->AsyncDepth = 1;
            out->IOPattern = 1;
            out->Protected = 0;

            //Extended coding options
            st = CheckExtBuffers_H265enc( out->ExtParam, out->NumExtParam );
            MFX_CHECK_STS(st);

            mfxExtCodingOptionHEVC* optsHEVC = (mfxExtCodingOptionHEVC*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_HEVCENC );
            if (optsHEVC != 0) {
                mfxExtBuffer HeaderCopy = optsHEVC->Header;
                memset( optsHEVC, 0, sizeof( mfxExtCodingOptionHEVC ));
                optsHEVC->Header = HeaderCopy;
                optsHEVC->Log2MaxCUSize = 1;
                optsHEVC->MaxCUDepth = 1;
                optsHEVC->QuadtreeTULog2MaxSize = 1;
                optsHEVC->QuadtreeTULog2MinSize = 1;
                optsHEVC->QuadtreeTUMaxDepthIntra = 1;
                optsHEVC->QuadtreeTUMaxDepthInter = 1;
                optsHEVC->AnalyzeChroma = 1;
                optsHEVC->SignBitHiding = 1;
                optsHEVC->RDOQuant = 1;
                optsHEVC->SAO      = 1;
                optsHEVC->SplitThresholdStrengthCUIntra = 1;
                optsHEVC->SplitThresholdStrengthTUIntra = 1;
                optsHEVC->SplitThresholdStrengthCUInter = 1;
                optsHEVC->SplitThresholdTabIndex        = 1;
                optsHEVC->IntraNumCand1_2 = 1;
                optsHEVC->IntraNumCand1_3 = 1;
                optsHEVC->IntraNumCand1_4 = 1;
                optsHEVC->IntraNumCand1_5 = 1;
                optsHEVC->IntraNumCand1_6 = 1;
                optsHEVC->IntraNumCand2_2 = 1;
                optsHEVC->IntraNumCand2_3 = 1;
                optsHEVC->IntraNumCand2_4 = 1;
                optsHEVC->IntraNumCand2_5 = 1;
                optsHEVC->IntraNumCand2_6 = 1;
                optsHEVC->WPP = 1;
                optsHEVC->GPB = 1;
                optsHEVC->PartModes = 1;
                optsHEVC->BPyramid = 1;
                optsHEVC->CostChroma = 1;
                optsHEVC->IntraChromaRDO = 1;
                optsHEVC->FastInterp = 1;
                optsHEVC->FastSkip = 1;
                optsHEVC->FastCbfMode = 1;
                optsHEVC->PuDecisionSatd = 1;
                optsHEVC->MinCUDepthAdapt = 1;
                optsHEVC->MaxCUDepthAdapt = 1;
                optsHEVC->CUSplitThreshold = 1;
                optsHEVC->DeltaQpMode = 1;
                optsHEVC->Enable10bit = 1;
                optsHEVC->CpuFeature = 1;
                optsHEVC->TryIntra   = 1;
                optsHEVC->FastAMPSkipME = 1;
                optsHEVC->FastAMPRD = 1;
                optsHEVC->SkipMotionPartition = 1;
                optsHEVC->SkipCandRD = 1;
                optsHEVC->FramesInParallel = 1;
                optsHEVC->NumTileCols = 1;
                optsHEVC->NumTileRows = 1;
            }

            mfxExtDumpFiles* optsDump = (mfxExtDumpFiles*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_DUMP );
            if (optsDump != 0) {
                mfxExtBuffer HeaderCopy = optsDump->Header;
                memset( optsDump, 0, sizeof( mfxExtDumpFiles ));
                optsDump->Header = HeaderCopy;
                optsDump->ReconFilename[0] = 1;
            }
            // ignore all reserved
        } else { //Check options for correctness
            const mfxVideoParam * const in = par_in;
            mfxVideoParam * const out = par_out;

            if ( in->mfx.CodecId != MFX_CODEC_HEVC)
                return MFX_ERR_UNSUPPORTED;

            st = CheckExtBuffers_H265enc( in->ExtParam, in->NumExtParam );
            MFX_CHECK_STS(st);
            st = CheckExtBuffers_H265enc( out->ExtParam, out->NumExtParam );
            MFX_CHECK_STS(st);

            const mfxExtCodingOptionHEVC* opts_in = (mfxExtCodingOptionHEVC*)GetExtBuffer(  in->ExtParam,  in->NumExtParam, MFX_EXTBUFF_HEVCENC );
            mfxExtCodingOptionHEVC*      opts_out = (mfxExtCodingOptionHEVC*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_HEVCENC );
            const mfxExtDumpFiles*    optsDump_in = (mfxExtDumpFiles*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_DUMP );
            mfxExtDumpFiles*         optsDump_out = (mfxExtDumpFiles*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_DUMP );

            if (opts_in           ) CHECK_EXTBUF_SIZE( *opts_in,            isInvalid)
                if (opts_out          ) CHECK_EXTBUF_SIZE( *opts_out,           isInvalid)
                    if (optsDump_in       ) CHECK_EXTBUF_SIZE( *optsDump_in,        isInvalid)
                        if (optsDump_out      ) CHECK_EXTBUF_SIZE( *optsDump_out,       isInvalid)
                            if (isInvalid)
                                return MFX_ERR_UNSUPPORTED;

            if ((opts_in==0) != (opts_out==0) ||
                (optsDump_in==0) != (optsDump_out==0))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            //if (in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 && in->mfx.FrameInfo.ChromaFormat == 0) { // because 0 == monochrome
            //    out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            //    out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            //    isCorrected ++;
            //} else

            if (((in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 || in->mfx.FrameInfo.FourCC == MFX_FOURCC_P010) && in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420) ||
                ((in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV16 || in->mfx.FrameInfo.FourCC == MFX_FOURCC_P210) && in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422)) {
                    out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
                    out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
            } else {
                out->mfx.FrameInfo.FourCC = 0;
                out->mfx.FrameInfo.ChromaFormat = 0;
                isInvalid ++;
            }

            if (in->Protected != 0)
                return MFX_ERR_UNSUPPORTED;
            out->Protected = 0;

            out->AsyncDepth = in->AsyncDepth;

            if ( (in->mfx.FrameInfo.Width & 15) || in->mfx.FrameInfo.Width > 8192 ) {
                out->mfx.FrameInfo.Width = 0;
                isInvalid ++;
            } else out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
            if ( (in->mfx.FrameInfo.Height & 15) || in->mfx.FrameInfo.Height > 4320 ) {
                out->mfx.FrameInfo.Height = 0;
                isInvalid ++;
            } else out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;

            // Check crops
            if (in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height && in->mfx.FrameInfo.Height) {
                out->mfx.FrameInfo.CropH = 0;
                isInvalid ++;
            } else out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;
            if (in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width && in->mfx.FrameInfo.Width) {
                out->mfx.FrameInfo.CropW = 0;
                isInvalid ++;
            } else  out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;
            if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width) {
                out->mfx.FrameInfo.CropX = 0;
                isInvalid ++;
            } else out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;
            if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height) {
                out->mfx.FrameInfo.CropY = 0;
                isInvalid ++;
            } else out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

            // Assume 420 checking horizontal crop to be even
            if ((in->mfx.FrameInfo.CropX & 1) && (in->mfx.FrameInfo.CropW & 1)) {
                if (out->mfx.FrameInfo.CropX == in->mfx.FrameInfo.CropX) // not to correct CropX forced to zero
                    out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX + 1;
                if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                    out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 1;
                isCorrected ++;
            } else if (in->mfx.FrameInfo.CropX & 1)
            {
                if (out->mfx.FrameInfo.CropX == in->mfx.FrameInfo.CropX) // not to correct CropX forced to zero
                    out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX + 1;
                if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                    out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 2;
                isCorrected ++;
            }
            else if (in->mfx.FrameInfo.CropW & 1) {
                if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                    out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 1;
                isCorrected ++;
            }

            // Assume 420 checking horizontal crop to be even
            if ((in->mfx.FrameInfo.CropY & 1) && (in->mfx.FrameInfo.CropH & 1)) {
                if (out->mfx.FrameInfo.CropY == in->mfx.FrameInfo.CropY) // not to correct CropY forced to zero
                    out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY + 1;
                if (out->mfx.FrameInfo.CropH) // can't decrement zero CropH
                    out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH - 1;
                isCorrected ++;
            } else if (in->mfx.FrameInfo.CropY & 1)
            {
                if (out->mfx.FrameInfo.CropY == in->mfx.FrameInfo.CropY) // not to correct CropY forced to zero
                    out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY + 1;
                if (out->mfx.FrameInfo.CropH) // can't decrement zero CropH
                    out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH - 2;
                isCorrected ++;
            }
            else if (in->mfx.FrameInfo.CropH & 1) {
                if (out->mfx.FrameInfo.CropH) // can't decrement zero CropH
                    out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH - 1;
                isCorrected ++;
            }

            //Check for valid framerate
            if (!in->mfx.FrameInfo.FrameRateExtN && in->mfx.FrameInfo.FrameRateExtD ||
                in->mfx.FrameInfo.FrameRateExtN && !in->mfx.FrameInfo.FrameRateExtD ||
                in->mfx.FrameInfo.FrameRateExtD && ((mfxF64)in->mfx.FrameInfo.FrameRateExtN / in->mfx.FrameInfo.FrameRateExtD) > 120) {
                    isInvalid ++;
                    out->mfx.FrameInfo.FrameRateExtN = out->mfx.FrameInfo.FrameRateExtD = 0;
            } else {
                out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
                out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;
            }

            switch(in->IOPattern) {
            case 0:
            case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
            case MFX_IOPATTERN_IN_VIDEO_MEMORY:
            case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
                out->IOPattern = in->IOPattern;
                break;
            default:
                if (in->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY) {
                    isCorrected ++;
                    out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
                } else if (in->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
                    isCorrected ++;
                    out->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
                } else {
                    isInvalid ++;
                    out->IOPattern = 0;
                }
            }

            out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
            out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

            if (in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
                in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN ) {
                    //out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                    //isCorrected ++;
                    out->mfx.FrameInfo.PicStruct = 0;
                    isInvalid ++;
            } else out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;

            out->mfx.BRCParamMultiplier = in->mfx.BRCParamMultiplier;

            out->mfx.CodecId = MFX_CODEC_HEVC;
            if (in->mfx.CodecProfile != MFX_PROFILE_UNKNOWN && in->mfx.CodecProfile != MFX_PROFILE_HEVC_MAIN &&
                (in->mfx.CodecProfile != MFX_PROFILE_HEVC_MAIN10)) {
                    isCorrected ++;
                    out->mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
            } else out->mfx.CodecProfile = in->mfx.CodecProfile;
            if (in->mfx.CodecLevel != MFX_LEVEL_UNKNOWN) {
                mfxI32 complevel = Check265Level(in->mfx.CodecLevel, in);
                if (complevel != in->mfx.CodecLevel) {
                    if (complevel == MFX_LEVEL_UNKNOWN)
                        isInvalid ++;
                    else
                        isCorrected ++;
                }
                out->mfx.CodecLevel = (mfxU16)complevel;
            }
            else out->mfx.CodecLevel = MFX_LEVEL_UNKNOWN;

            out->mfx.NumThread = in->mfx.NumThread;

        
#ifdef MFX_VA
            if (in->mfx.TargetUsage != MFX_TARGETUSAGE_7) {
                isCorrected ++;
                out->mfx.TargetUsage = MFX_TARGETUSAGE_7;
            }
            else out->mfx.TargetUsage = in->mfx.TargetUsage;
#else
            if (in->mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED) {
                isCorrected ++;
                out->mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
            }
            else out->mfx.TargetUsage = in->mfx.TargetUsage;
#endif

            out->mfx.GopPicSize = in->mfx.GopPicSize;

#ifdef MFX_VA   // Gacc supports up to 8 B frames now (otherwise it exceeds surface amount)
            if (in->mfx.GopRefDist > 8) {
                out->mfx.GopRefDist = 8;
                isCorrected ++;
            }
            else out->mfx.GopRefDist = in->mfx.GopRefDist;
#else
            if (in->mfx.GopRefDist > H265_MAXREFDIST) {
                out->mfx.GopRefDist = H265_MAXREFDIST;
                isCorrected ++;
            }
            else out->mfx.GopRefDist = in->mfx.GopRefDist;
#endif

            if (in->mfx.NumRefFrame > MAX_NUM_REF_IDX) {
                out->mfx.NumRefFrame = MAX_NUM_REF_IDX;
                isCorrected ++;
            }
            else out->mfx.NumRefFrame = in->mfx.NumRefFrame;

            if ((in->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT)) != in->mfx.GopOptFlag) {
                out->mfx.GopOptFlag = 0;
                isInvalid ++;
            } else out->mfx.GopOptFlag = in->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT);

            out->mfx.IdrInterval = in->mfx.IdrInterval;

            // NumSlice will be checked again after Log2MaxCUSize
            out->mfx.NumSlice = in->mfx.NumSlice;
            if ( in->mfx.NumSlice > 0 && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.Width /*&& opts_out && opts_out->Log2MaxCUSize*/)
            {
                mfxU16 rnd = (1 << 4) - 1, shft = 4;
                if (in->mfx.NumSlice > ((out->mfx.FrameInfo.Height+rnd)>>shft) * ((out->mfx.FrameInfo.Width+rnd)>>shft) )
                {
                    out->mfx.NumSlice = 0;
                    isInvalid ++;
                }
            }
            out->mfx.NumRefFrame = in->mfx.NumRefFrame;

            if (in->mfx.EncodedOrder != 0) {
                isInvalid ++;
            }
            out->mfx.EncodedOrder = 0;

            if (in->mfx.RateControlMethod != 0 &&
                in->mfx.RateControlMethod != MFX_RATECONTROL_CBR && in->mfx.RateControlMethod != MFX_RATECONTROL_VBR && in->mfx.RateControlMethod != MFX_RATECONTROL_AVBR && in->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                    out->mfx.RateControlMethod = 0;
                    isInvalid ++;
            } else out->mfx.RateControlMethod = in->mfx.RateControlMethod;

            RcParams rcParams;
            SetCalcParams(&rcParams, in);

            if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
                out->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
                    if (out->mfx.FrameInfo.Width && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.FrameRateExtD && rcParams.TargetKbps) {
                        // last denominator 2000 gives about 0.75 Mbps for 1080p x 60
                        Ipp32s minBitRate = (Ipp32s)((mfxF64)out->mfx.FrameInfo.Width * out->mfx.FrameInfo.Height * 12 // size of raw image (luma + chroma 420) in bits
                            * out->mfx.FrameInfo.FrameRateExtN / out->mfx.FrameInfo.FrameRateExtD / 1000 / 2000);
                        if (minBitRate > rcParams.TargetKbps) {
                            rcParams.TargetKbps = minBitRate;
                            isCorrected ++;
                        }
                        Ipp32s AveFrameKB = rcParams.TargetKbps * out->mfx.FrameInfo.FrameRateExtD / out->mfx.FrameInfo.FrameRateExtN / 8;
                        if (rcParams.BufferSizeInKB != 0 && rcParams.BufferSizeInKB < 2 * AveFrameKB) {
                            rcParams.BufferSizeInKB = 2 * AveFrameKB;
                            isCorrected ++;
                        }
                        if (rcParams.InitialDelayInKB != 0 && rcParams.InitialDelayInKB < AveFrameKB) {
                            rcParams.InitialDelayInKB = AveFrameKB;
                            isCorrected ++;
                        }
                    }

                    if (rcParams.MaxKbps != 0 && rcParams.MaxKbps < rcParams.TargetKbps) {
                        rcParams.MaxKbps = rcParams.TargetKbps;
                    }
            }
            // check for correct QPs for const QP mode
            else if (out->mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
                if (in->mfx.QPI > 51) {
                    out->mfx.QPI = 0;
                    isInvalid ++;
                } else out->mfx.QPI = in->mfx.QPI;
                if (in->mfx.QPP > 51) {
                    out->mfx.QPP = 0;
                    isInvalid ++;
                } else out->mfx.QPP = in->mfx.QPP;
                if (in->mfx.QPB > 51) {
                    out->mfx.QPB = 0;
                    isInvalid ++;
                } else out->mfx.QPB = in->mfx.QPB;
            }
            else {
                out->mfx.Accuracy = in->mfx.Accuracy;
                out->mfx.Convergence = in->mfx.Convergence;
            }

            GetCalcParams(out, &rcParams, out->mfx.RateControlMethod);

            if (opts_in) {
                if ((opts_in->Log2MaxCUSize != 0 && opts_in->Log2MaxCUSize < 4) || opts_in->Log2MaxCUSize > 6) {
                    opts_out->Log2MaxCUSize = 0;
                    isInvalid ++;
                } else opts_out->Log2MaxCUSize = opts_in->Log2MaxCUSize;

                if ( (opts_in->MaxCUDepth > 5) ||
                    (opts_out->Log2MaxCUSize!=0 && opts_in->MaxCUDepth + 1 > opts_out->Log2MaxCUSize) ) {
                        opts_out->MaxCUDepth = 0;
                        isInvalid ++;
                } else opts_out->MaxCUDepth = opts_in->MaxCUDepth;

                if (opts_out->Log2MaxCUSize && opts_out->MaxCUDepth) {
                    int MinCUSize = 1 << (opts_out->Log2MaxCUSize - opts_out->MaxCUDepth + 1);
                    if ((out->mfx.FrameInfo.Width | out->mfx.FrameInfo.Height) & (MinCUSize-1)) {
                        opts_out->MaxCUDepth = 0;
                        isInvalid ++;
                    }
                }

                if ( (opts_in->QuadtreeTULog2MaxSize > 5) ||
                    (opts_out->Log2MaxCUSize!=0 && opts_in->QuadtreeTULog2MaxSize > opts_out->Log2MaxCUSize) ) {
                        opts_out->QuadtreeTULog2MaxSize = 0;
                        isInvalid ++;
                } else opts_out->QuadtreeTULog2MaxSize = opts_in->QuadtreeTULog2MaxSize;

                if ( (opts_in->QuadtreeTULog2MinSize == 1) ||
                    (opts_out->Log2MaxCUSize!=0 && opts_in->QuadtreeTULog2MinSize > opts_out->Log2MaxCUSize) ) {
                        opts_out->QuadtreeTULog2MinSize = 0;
                        isInvalid ++;
                } else opts_out->QuadtreeTULog2MinSize = opts_in->QuadtreeTULog2MinSize;

                if (opts_out->QuadtreeTULog2MinSize)
                    if ((out->mfx.FrameInfo.Width | out->mfx.FrameInfo.Height) & ((1<<opts_out->QuadtreeTULog2MinSize)-1)) {
                        opts_out->QuadtreeTULog2MinSize = 0;
                        isInvalid ++;
                    }

                    if ( (opts_in->QuadtreeTUMaxDepthIntra > 5) ||
                        (opts_out->Log2MaxCUSize!=0 && opts_in->QuadtreeTUMaxDepthIntra + 1 > opts_out->Log2MaxCUSize) ) {
                            opts_out->QuadtreeTUMaxDepthIntra = 0;
                            isInvalid ++;
                    } else opts_out->QuadtreeTUMaxDepthIntra = opts_in->QuadtreeTUMaxDepthIntra;

                    if ( (opts_in->QuadtreeTUMaxDepthInter > 5) ||
                        (opts_out->Log2MaxCUSize!=0 && opts_in->QuadtreeTUMaxDepthInter + 1 > opts_out->Log2MaxCUSize) ) {
                            opts_out->QuadtreeTUMaxDepthInter = 0;
                            isInvalid ++;
                    } else opts_out->QuadtreeTUMaxDepthInter = opts_in->QuadtreeTUMaxDepthInter;

                    opts_out->TryIntra = opts_in->TryIntra;
                    opts_out->CmIntraThreshold = opts_in->CmIntraThreshold;
                    opts_out->TUSplitIntra = opts_in->TUSplitIntra;
                    opts_out->CUSplit = opts_in->CUSplit;
                    opts_out->IntraAngModes = opts_in->IntraAngModes;
                    opts_out->IntraAngModesP = opts_in->IntraAngModesP;
                    opts_out->IntraAngModesBRef = opts_in->IntraAngModesBRef;
                    opts_out->IntraAngModesBnonRef = opts_in->IntraAngModesBnonRef;
                    opts_out->HadamardMe = opts_in->HadamardMe;
                    opts_out->TMVP = opts_in->TMVP;
                    opts_out->EnableCm = opts_in->EnableCm;
                    opts_out->SaoOpt = opts_in->SaoOpt;
                    opts_out->PatternIntPel = opts_in->PatternIntPel;
                    opts_out->PatternSubPel = opts_in->PatternSubPel;
                    opts_out->ForceNumThread = opts_in->ForceNumThread;
                    opts_out->NumBiRefineIter = opts_in->NumBiRefineIter;
                    opts_out->CUSplitThreshold = opts_in->CUSplitThreshold;
                    opts_out->DeltaQpMode = opts_in->DeltaQpMode;
                    opts_out->CpuFeature = opts_in->CpuFeature;
                    opts_out->FramesInParallel         = opts_in->FramesInParallel;

                    CHECK_OPTION(opts_in->AnalyzeChroma, opts_out->AnalyzeChroma, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->SignBitHiding, opts_out->SignBitHiding, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->RDOQuant, opts_out->RDOQuant, isInvalid);            /* tri-state option */
                    CHECK_OPTION(opts_in->GPB, opts_out->GPB, isInvalid);            /* tri-state option */
                    CHECK_OPTION(opts_in->SAO, opts_out->SAO, isInvalid);            /* tri-state option */
                    CHECK_OPTION(opts_in->WPP, opts_out->WPP, isInvalid);       /* tri-state option */
                    CHECK_OPTION(opts_in->BPyramid, opts_out->BPyramid, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->Deblocking, opts_out->Deblocking, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->RDOQuantChroma, opts_out->RDOQuantChroma, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->RDOQuantCGZ, opts_out->RDOQuantCGZ, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->CostChroma, opts_out->CostChroma, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->IntraChromaRDO, opts_out->IntraChromaRDO, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->FastInterp, opts_out->FastInterp, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->FastSkip, opts_out->FastSkip, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->FastCbfMode, opts_out->FastCbfMode, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->PuDecisionSatd, opts_out->PuDecisionSatd, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->MinCUDepthAdapt, opts_out->MinCUDepthAdapt, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->MaxCUDepthAdapt, opts_out->MaxCUDepthAdapt, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->Enable10bit, opts_out->Enable10bit, isInvalid);  /* tri-state option */
                    CHECK_OPTION(opts_in->SkipCandRD, opts_out->SkipCandRD, isInvalid);  /* tri-state option */

                    if (opts_in->PartModes > 3) {
                        opts_out->PartModes = 0;
                        isInvalid ++;
                    } else opts_out->PartModes = opts_in->PartModes;

                    if (opts_out->SAO == MFX_CODINGOPTION_ON && in->mfx.NumSlice > 1) { // switch off SAO for now, because of inter-slice deblocking
                        opts_out->SAO = MFX_CODINGOPTION_OFF;
                        isCorrected++;
                    }

                    if (opts_in->SplitThresholdStrengthCUIntra > 4) {
                        opts_out->SplitThresholdStrengthCUIntra = 0;
                        isInvalid ++;
                    } else opts_out->SplitThresholdStrengthCUIntra = opts_in->SplitThresholdStrengthCUIntra;

                    if (opts_in->SplitThresholdStrengthTUIntra > 4) {
                        opts_out->SplitThresholdStrengthTUIntra = 0;
                        isInvalid ++;
                    } else opts_out->SplitThresholdStrengthTUIntra = opts_in->SplitThresholdStrengthTUIntra;

                    if (opts_in->SplitThresholdStrengthCUInter > 4) {
                        opts_out->SplitThresholdStrengthCUInter = 0;
                        isInvalid ++;
                    } else opts_out->SplitThresholdStrengthCUInter = opts_in->SplitThresholdStrengthCUInter;

                    if (opts_in->SplitThresholdTabIndex > 3) {
                        opts_out->SplitThresholdTabIndex = 0;
                        isInvalid ++;
                    } else opts_out->SplitThresholdTabIndex = opts_in->SplitThresholdTabIndex;

#define CHECK_NUMCAND(field)                            \
    if (opts_in->field > maxnum) {              \
    opts_out->field = 0;                    \
    isInvalid ++;                           \
    } else opts_out->field = opts_in->field

                    mfxU16 maxnum = 35;
                    CHECK_NUMCAND(IntraNumCand0_2);
                    CHECK_NUMCAND(IntraNumCand0_3);
                    CHECK_NUMCAND(IntraNumCand0_4);
                    CHECK_NUMCAND(IntraNumCand0_5);
                    CHECK_NUMCAND(IntraNumCand0_6);
                    CHECK_NUMCAND(IntraNumCand1_2);
                    CHECK_NUMCAND(IntraNumCand1_3);
                    CHECK_NUMCAND(IntraNumCand1_4);
                    CHECK_NUMCAND(IntraNumCand1_5);
                    CHECK_NUMCAND(IntraNumCand1_6);
                    CHECK_NUMCAND(IntraNumCand2_2);
                    CHECK_NUMCAND(IntraNumCand2_3);
                    CHECK_NUMCAND(IntraNumCand2_4);
                    CHECK_NUMCAND(IntraNumCand2_5);
                    CHECK_NUMCAND(IntraNumCand2_6);

#undef CHECK_NUMCAND

                    // check again Numslice using Log2MaxCUSize
                    if ( out->mfx.NumSlice > 0 && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.Width && opts_out->Log2MaxCUSize) {
                        mfxU16 rnd = (1 << opts_out->Log2MaxCUSize) - 1, shft = opts_out->Log2MaxCUSize;
                        if (out->mfx.NumSlice > ((out->mfx.FrameInfo.Height+rnd)>>shft) * ((out->mfx.FrameInfo.Width+rnd)>>shft) ) {
                            out->mfx.NumSlice = 0;
                            isInvalid ++;
                        }
                    }

                    if (opts_in->NumTileCols > MAX_NUM_TILE_COLUMNS ||
                        (opts_in->NumTileCols > 1 && (out->mfx.NumSlice > 1 || opts_out->FramesInParallel > 1))) {
                            opts_out->NumTileCols = 1;
                            isInvalid ++;
                    } else opts_out->NumTileCols = opts_in->NumTileCols;

                    if (opts_in->NumTileRows > MAX_NUM_TILE_ROWS ||
                        (opts_in->NumTileRows > 1 && (out->mfx.NumSlice > 1 || opts_out->FramesInParallel > 1))) {
                            opts_out->NumTileRows = 1;
                            isInvalid ++;
                    } else opts_out->NumTileRows = opts_in->NumTileRows;
            } // EO mfxExtCodingOptionHEVC

            if (optsDump_in) {
                vm_string_strncpy_s(optsDump_out->ReconFilename, sizeof(optsDump_out->ReconFilename), optsDump_in->ReconFilename, sizeof(optsDump_out->ReconFilename)-1);
            }

            // reserved for any case
            ////// Assume 420 checking vertical crop to be even for progressive PicStruct
            ////mfxU16 cropSampleMask, correctCropTop, correctCropBottom;
            ////if ((in->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))  == MFX_PICSTRUCT_PROGRESSIVE)
            ////    cropSampleMask = 1;
            ////else
            ////    cropSampleMask = 3;
            ////
            ////correctCropTop = (in->mfx.FrameInfo.CropY + cropSampleMask) & ~cropSampleMask;
            ////correctCropBottom = (in->mfx.FrameInfo.CropY + out->mfx.FrameInfo.CropH) & ~cropSampleMask;
            ////
            ////if ((in->mfx.FrameInfo.CropY & cropSampleMask) || (out->mfx.FrameInfo.CropH & cropSampleMask)) {
            ////    if (out->mfx.FrameInfo.CropY == in->mfx.FrameInfo.CropY) // not to correct CropY forced to zero
            ////        out->mfx.FrameInfo.CropY = correctCropTop;
            ////    if (correctCropBottom >= out->mfx.FrameInfo.CropY)
            ////        out->mfx.FrameInfo.CropH = correctCropBottom - out->mfx.FrameInfo.CropY;
            ////    else // CropY < cropSample
            ////        out->mfx.FrameInfo.CropH = 0;
            ////    isCorrected ++;
            ////}


        }

        if (isInvalid)
            return MFX_ERR_UNSUPPORTED;
        if (isCorrected)
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR1(par)
        MFX_CHECK_NULL_PTR1(request)

        // check for valid IOPattern
        mfxU16 IOPatternIn = par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY);
    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0) ||
        (IOPatternIn != MFX_IOPATTERN_IN_VIDEO_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_SYSTEM_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_OPAQUE_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (par->Protected != 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxU16 nFrames = IPP_MIN(par->mfx.GopRefDist, H265_MAXREFDIST); // 1 for current and GopRefDist - 1 for reordering

    if( nFrames == 0 ) {
        // curr + number of B-frames from target usage
        nFrames = tab_tuGopRefDist[par->mfx.TargetUsage];
    }

    //if(LookAhead)
#if defined(MFX_ENABLE_H265_PAQ)
    //if(m_enc->m_videoParam.preEncMode)
    {
        mfxVideoParam tmpParam = *par;
        nFrames = (mfxU16) AsyncRoutineEmulator(tmpParam).GetTotalGreediness() + par->AsyncDepth - 1 + 10;
    }
#endif
    request->NumFrameMin = nFrames;
    request->NumFrameSuggested = IPP_MAX(nFrames,par->AsyncDepth);
    request->Info = par->mfx.FrameInfo;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY){
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    } else if (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    } else {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::GetEncodeStat(mfxEncodeStat *stat)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(stat)
        memset(stat, 0, sizeof(mfxEncodeStat));
    stat->NumCachedFrame = m_frameCountBufferedSync; //(mfxU32)m_inFrames.size();
    stat->NumBit = m_totalBits;
    stat->NumFrame = m_encodedFrames;

    return MFX_ERR_NONE;
}


mfxStatus MFXVideoENCODEH265::AcceptFrameHelper(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *inputSurface, mfxBitstream *bs)
{
    mfxStatus st;
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxFrameSurface1 *surface = inputSurface;

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    // [1] inputSurface can be opaque. Get real surface from core
    surface = GetOriginalSurface(inputSurface);

    // [2] pre-work for input
    bool locked = false;
    if (surface) {
        if (surface->Info.FourCC != MFX_FOURCC_NV12 && 
            surface->Info.FourCC != MFX_FOURCC_NV16 && 
            surface->Info.FourCC != MFX_FOURCC_P010 && 
            surface->Info.FourCC != MFX_FOURCC_P210)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

        if (m_useAuxInput) { // copy from d3d to internal frame in system memory

            // Lock internal. FastCopy to use locked, to avoid additional lock/unlock
            st = m_core->LockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
            MFX_CHECK_STS(st);

            st = m_core->DoFastCopyWrapper(&m_auxInput,
                MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                surface,
                MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
            MFX_CHECK_STS(st);

            m_core->DecreaseReference(&(surface->Data)); // do it here

            m_auxInput.Data.FrameOrder = surface->Data.FrameOrder;
            m_auxInput.Data.TimeStamp = surface->Data.TimeStamp;
            surface = &m_auxInput; // replace input pointer
        } else {
            if (surface->Data.Y == 0 && surface->Data.U == 0 &&
                surface->Data.V == 0 && surface->Data.A == 0)
            {
                st = m_core->LockExternalFrame(surface->Data.MemId, &(surface->Data));
                if (st == MFX_ERR_NONE)
                    locked = true;
                else
                    return st;
            }
        }

        if (!surface->Data.Y || !surface->Data.UV || surface->Data.Pitch > 0x7fff ||
            surface->Data.Pitch < m_mfxParam.mfx.FrameInfo.Width )
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }


    // [3] 
    mfxRes = AcceptFrame(surface, bs);


    // [4] post-work for input
    if(surface) {
        m_core->DecreaseReference(&(surface->Data));

        if ( m_useAuxInput ) {
            m_core->UnlockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
        } else if (locked) {
            m_core->UnlockExternalFrame(surface->Data.MemId, &(surface->Data));
        }

        m_frameCount ++;
    }


    // statistics update
    if (surface) {
        if (mfxRes == MFX_ERR_MORE_DATA) m_frameCountBuffered++;
    } else {
        if (mfxRes == MFX_ERR_NONE) m_frameCountBuffered--;
    }

    // no problem
    if (mfxRes == MFX_ERR_MORE_DATA) {
        mfxRes = MFX_ERR_NONE;
    }

    return mfxRes;
}


// --------------------------------------------------------
//   Utils
// --------------------------------------------------------

void ReorderBiFrames(TaskIter inBegin, TaskIter inEnd, TaskList &in, TaskList &out, Ipp32s layer = 1)
{
    if (inBegin == inEnd)
        return;
    TaskIter pivot = inBegin;
    std::advance(pivot, (std::distance(inBegin, inEnd) - 1) / 2);
    (*pivot)->m_frameOrigin->m_pyramidLayer = layer;
    TaskIter rightHalf = pivot;
    ++rightHalf;
    if (inBegin == pivot)
        ++inBegin;
    out.splice(out.end(), in, pivot);
    ReorderBiFrames(inBegin, rightHalf, in, out, layer + 1);
    ReorderBiFrames(rightHalf, inEnd, in, out, layer + 1);
}


void ReorderBiFramesLongGop(TaskIter inBegin, TaskIter inEnd, TaskList &in, TaskList &out)
{
    VM_ASSERT(std::distance(inBegin, inEnd) == 15);

    // 3 anchors + 3 mini-pyramids
    for (Ipp32s i = 0; i < 3; i++) {
        TaskIter anchor = inBegin;
        std::advance(anchor, 3);
        TaskIter afterAnchor = anchor;
        ++afterAnchor;
        (*anchor)->m_frameOrigin->m_pyramidLayer = 2;
        out.splice(out.end(), in, anchor);
        ReorderBiFrames(inBegin, afterAnchor, in, out, 3);
        inBegin = afterAnchor;
    }
    // last 4th mini-pyramid
    ReorderBiFrames(inBegin, inEnd, in, out, 3);
}

void ReorderFrames(TaskList &input, TaskList &reordered, const H265VideoParam &param, Ipp32s endOfStream)
{
    Ipp32s closedGop = param.GopClosedFlag;
    Ipp32s strictGop = param.GopStrictFlag;
    Ipp32s biPyramid = param.BiPyramidLayers > 1;

    if (input.empty())
        return;

    TaskIter anchor = input.begin();
    TaskIter end = input.end();
    while (anchor != end && (*anchor)->m_frameOrigin->m_picCodeType == MFX_FRAMETYPE_B)
        ++anchor;
    if (anchor == end && !endOfStream)
        return; // minigop is not accumulated yet
    if (anchor == input.begin()) {
        reordered.splice(reordered.end(), input, anchor); // lone anchor frame
        return;
    }

    // B frames present
    // process different situations:
    //   (a) B B B <end_of_stream> -> B B B    [strictGop=true ]
    //   (b) B B B <end_of_stream> -> B B P    [strictGop=false]
    //   (c) B B B <new_sequence>  -> B B B    [strictGop=true ]
    //   (d) B B B <new_sequence>  -> B B P    [strictGop=false]
    //   (e) B B P                 -> B B P
    bool anchorExists = true;
    if (anchor == end) {
        if (strictGop)
            anchorExists = false; // (a) encode B frames without anchor
        else
            (*--anchor)->m_frameOrigin->m_picCodeType = MFX_FRAMETYPE_P; // (b) use last B frame of stream as anchor
    }
    else {
        H265Frame *anchorFrm = (*anchor)->m_frameOrigin;
        if (anchorFrm->m_picCodeType == MFX_FRAMETYPE_I && (anchorFrm->m_isIdrPic || closedGop)) {
            if (strictGop)
                anchorExists = false; // (c) encode B frames without anchor
            else
                (*--anchor)->m_frameOrigin->m_picCodeType = MFX_FRAMETYPE_P; // (d) use last B frame of current sequence as anchor
        }
    }

    // setup number of B frames
    Ipp32s numBiFrames = (Ipp32s)std::distance(input.begin(), anchor);
    for (TaskIter i = input.begin(); i != anchor; ++i)
        (*i)->m_frameOrigin->m_biFramesInMiniGop = numBiFrames;

    // reorder anchor frame
    TaskIter afterBiFrames = anchor;
    if (anchorExists) {
        (*anchor)->m_frameOrigin->m_pyramidLayer = 0; // anchor frames are from layer=0
        (*anchor)->m_frameOrigin->m_biFramesInMiniGop = numBiFrames;
        ++afterBiFrames;
        reordered.splice(reordered.end(), input, anchor);
    }

    if (biPyramid)
        if (numBiFrames == 15 && param.longGop)
            ReorderBiFramesLongGop(input.begin(), afterBiFrames, input, reordered); // B frames in long pyramid order
        else
            ReorderBiFrames(input.begin(), afterBiFrames, input, reordered); // B frames in pyramid order
    else
        reordered.splice(reordered.end(), input, input.begin(), afterBiFrames); // no pyramid, B frames in input order
}


Task* FindOldestOutputTask(TaskList & encodeQueue)
{
    TaskIter begin = encodeQueue.begin();
    TaskIter end = encodeQueue.end();

    if (begin == end)
        return NULL;

    TaskIter oldest = begin;
    for (++begin; begin != end; ++begin) {
        if ( (*oldest)->m_encOrder > (*begin)->m_encOrder)
            oldest = begin;
    }

    return *oldest;

} // 

// --------------------------------------------------------
//              MFXVideoENCODEH265
// --------------------------------------------------------

mfxStatus MFXVideoENCODEH265::AcceptFrame(mfxFrameSurface1 *surface, mfxBitstream *mfxBS)
{
    // STAGE:: [ACCEPT]
    if (surface) {
        H265Frame* inputFrame = InsertInputFrame(surface); // copy surface -> inputFrame (with padding) and returns ptr to inputFrame
        MFX_CHECK_NULL_PTR1(inputFrame);

#ifdef MFX_ENABLE_WATERMARK
        // TODO: find appropriate H265FrameEncoder with m_watermark
        // m_watermark->Apply(inputFrame->y, inputFrame->uv, inputFrame->pitch_luma, surface->Info.Width, surface->Info.Height);
        return MFX_ERR_UNSUPPORTED;
#endif

        ConfigureInputFrame(inputFrame);

        // update counters
        m_frameOrder++;
        m_lastTimeStamp = inputFrame->m_timeStamp;
        if (inputFrame->m_isIdrPic)
            m_frameOrderOfLastIdr = inputFrame->m_frameOrder;
        if (inputFrame->m_picCodeType & MFX_FRAMETYPE_I)
            m_frameOrderOfLastIntra = inputFrame->m_frameOrder;
        if (inputFrame->m_picCodeType != MFX_FRAMETYPE_B) {
            m_frameOrderOfLastAnchor = inputFrame->m_frameOrder;
            m_miniGopCount++;
        }
    }

    //-----------------------------------------------------
    // aya: tmp hack!!! buffering here
    /*if (m_videoParam.preEncMode) {
        mfxStatus stsPreEnc = PreEncAnalysis();
        stsPreEnc;
    }*/
    //-----------------------------------------------------

    std::list<Task*> & inputQueue = m_videoParam.preEncMode ? m_lookaheadQueue : m_inputQueue;
    // STAGE:: [REORDER]
    // prepare next frame for encoding
    if (m_reorderedQueue.empty()) {
        ReorderFrames(inputQueue, m_reorderedQueue, m_videoParam, surface == NULL);
    }

    if (!m_reorderedQueue.empty()) {
        ConfigureEncodeFrame(m_reorderedQueue.front());
        m_lastEncOrder = m_reorderedQueue.front()->m_encOrder;

        m_dpb.splice(m_dpb.end(), m_reorderedQueue, m_reorderedQueue.begin());
        m_encodeQueue.push_back(m_dpb.back());
    }

    // general criteria to continue encoding
    //bool isGo = !m_encodeQueue.empty() ||
    //    inputQueue.size() >= (size_t)m_videoParam.GopRefDist ||
    //    !surface && !inputQueue.empty();

    // special criterion for "instantaneous" first IDR frame
    /*if (!isGo && mfxBS && 1 == m_inputQueue.size() && m_inputQueue.front()->m_frameOrigin->m_isIdrPic)
        isGo = true;*/

    if (!mfxBS)
        return MFX_ERR_NONE;
        //return isGo ? MFX_ERR_NONE : MFX_ERR_MORE_DATA; // means delay

    /*if (!isGo)
        return MFX_ERR_MORE_DATA;*/

    return MFX_ERR_NONE;

} //


mfxStatus MFXVideoENCODEH265::AddNewOutputTask(int& encIdx)
{
    if (m_encodeQueue.empty()) {
        encIdx = -1;
        return MFX_ERR_MORE_DATA;
    }

    for (encIdx = 0; encIdx < (int)m_frameEncoder.size(); encIdx++){
        if(m_frameEncoder[encIdx]->IsFree()){
            break;
        }
    }

    if (encIdx == (int)m_frameEncoder.size()) {
        encIdx = -1; // not found
        return MFX_TASK_BUSY;
    }

    //accusition resiources
    m_frameEncoder[encIdx]->SetFree(false);

    // OK, start binding
    //m_encodeQueue.splice(m_encodeQueue.end(), m_lookaheadQueue, m_lookaheadQueue.begin());
    VM_ASSERT(m_encodeQueue.size() >= 1);

    // lasy initialization of task
    Task* task = m_encodeQueue.front();

    task->m_encIdx = encIdx;

#if defined (MFX_VA)
    if (m_videoParam.enableCmFlag) {
        task->m_extParam = static_cast<void*>( m_FeiCtx );
    }
#endif

    if ( m_brc && m_videoParam.m_framesInParallel == 1 ) {
        task->m_sliceQpY = (Ipp8s)m_brc->GetQP(m_videoParam, task->m_frameOrigin);
    }

    Ipp32s numCtb = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;
    memset(&task->m_lcuQps[0], task->m_sliceQpY, sizeof(task->m_sliceQpY)*numCtb);

    // setup slices
    H265Slice *currSlices = task->m_slices;
    for (Ipp8u i = 0; i < m_videoParam.NumSlices; i++) {
        (currSlices + i)->slice_qp_delta = task->m_sliceQpY - m_pps.init_qp;
        SetAllLambda(m_videoParam, (currSlices + i), task->m_sliceQpY, task->m_frameOrigin );
    }

#if defined (MFX_ENABLE_H265_PAQ)
    if (m_videoParam.preEncMode && m_videoParam.UseDQP) {
        Ipp32s poc = task->m_frameOrigin->m_poc;
        for (Ipp32s ctb = 0; ctb < numCtb; ctb++) {
            Ipp32s ctb_adapt = ctb;
            Ipp32s locPoc = poc % m_preEnc.m_histLength;
            if (m_videoParam.preEncMode != 2) // no pure CALQ 
                task->m_lcuQps[ctb] += (Ipp8s)m_preEnc.m_acQPMap[locPoc].getDQP(ctb_adapt);
        }
        for (Ipp8u i = 0; i < m_videoParam.NumSlices; i++)
            if (m_videoParam.preEncMode && m_videoParam.UseDQP)
                UpdateAllLambda(task);
    }
#endif

    mfxStatus sts = m_frameEncoder[encIdx]->SetEncodeTask(task);
    MFX_CHECK_STS(sts);
    m_outputQueue.splice(m_outputQueue.end(), m_encodeQueue, m_encodeQueue.begin());

    vm_interlocked_cas32( &(m_outputQueue.back()->m_statusReport), 1, 0);
    //printf("\n submitted encOrder %i \n", task->m_encOrder);fflush(stderr);

    return MFX_ERR_NONE;

} // 


mfxStatus MFXVideoENCODEH265::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par)

        par->mfx = m_mfxParam.mfx;
    par->Protected = 0;
    par->AsyncDepth = m_mfxParam.AsyncDepth;
    par->IOPattern = m_mfxParam.IOPattern;

    mfxExtCodingOptionHEVC* opts_hevc = (mfxExtCodingOptionHEVC*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_HEVCENC );
    if (opts_hevc)
        *opts_hevc = m_mfxHEVCOpts;

    return MFX_ERR_NONE;
}


mfxStatus MFXVideoENCODEH265::GetFrameParam(mfxFrameParam * /*par*/)
{
    return MFX_ERR_UNSUPPORTED;
}


mfxStatus MFXVideoENCODEH265::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint)
{
    pEntryPoint->pRoutine = TaskRoutine;
    pEntryPoint->pCompleteProc = TaskCompleteProc;
    pEntryPoint->pState = this;
    pEntryPoint->requiredNumThreads = m_mfxParam.mfx.NumThread;
    pEntryPoint->pRoutineName = "EncodeHEVC";

    mfxStatus status = EncodeFrameCheck(ctrl, surface, bs, reordered_surface, pInternalParams);

    mfxFrameSurface1 *realSurface = GetOriginalSurface(surface);

    if (surface && !realSurface)
        return MFX_ERR_UNDEFINED_BEHAVIOR;


    if (MFX_ERR_NONE == status || MFX_ERR_MORE_DATA_RUN_TASK == status || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == status || MFX_ERR_MORE_BITSTREAM == status) {
        // lock surface. If input surface is opaque core will lock both opaque and associated realSurface
        if (realSurface) {
            m_core->IncreaseReference(&(realSurface->Data));
        }

        H265EncodeTaskInputParams *m_pTaskInputParams = (H265EncodeTaskInputParams*)H265_Malloc(sizeof(H265EncodeTaskInputParams));

        // MFX_ERR_MORE_DATA_RUN_TASK means that frame will be buffered and will be encoded later. 
        // Output bitstream isn't required for this task. it is marker for TaskRoutine() and TaskComplete()
        m_pTaskInputParams->bs = (status == MFX_ERR_MORE_DATA_RUN_TASK) ? 0 : bs;
        m_pTaskInputParams->ctrl = ctrl;
        m_pTaskInputParams->surface = surface;

        m_pTaskInputParams->m_taskID = m_taskID++;
        m_pTaskInputParams->m_doStage = 0;
        m_pTaskInputParams->m_targetTask = NULL;
        m_pTaskInputParams->m_threadCount = 0;
        m_pTaskInputParams->m_outputQueueSize = 0;
        m_pTaskInputParams->m_reencode = 0;

        pEntryPoint->pParam = m_pTaskInputParams;
    }

    return status;

} // 


mfxStatus MFXVideoENCODEH265::EncSolver(Task* task, volatile Ipp32u* onExitEvent)
{
    int encIdx = task->m_encIdx;
    H265FrameEncoder* fenc = m_frameEncoder[encIdx];
    mfxI32 thread_number;
    mfxStatus sts = MFX_TASK_BUSY;

    mfxU8 bitDepth = m_videoParam.bitDepthLuma > 8;

    sts = 
        bitDepth ?
        fenc->EncodeThread<Ipp16u>(thread_number, onExitEvent, m_semaphore):
        fenc->EncodeThread<Ipp8u>(thread_number, onExitEvent, m_semaphore);

    return sts;

} //


// wait full complete of current task
#if defined( _WIN32) || defined(_WIN64)
#define thread_sleep(nms) Sleep(nms)
#else
#define thread_sleep(nms) _mm_pause()
#endif
#define x86_pause() _mm_pause()

void MFXVideoENCODEH265::SyncOnTaskCompletion(Task *task, mfxBitstream *mfxBs, void *pParam)
{
    H265EncodeTaskInputParams *inputParam = (H265EncodeTaskInputParams *)pParam;

    if (vm_interlocked_cas32( &(task->m_statusReport), 2, 1) == 1) {

        // only one thread should go further, others turned around here
        if (!(task->m_frameRecon->m_codedRow == (Ipp32s)m_videoParam.PicHeightInCtbs)) {
            vm_interlocked_cas32( &(task->m_statusReport), 1, 2);
            return;
        }

        H265FrameEncoder *frameEnc = m_frameEncoder[task->m_encIdx];

        // STAGE::[BS UPDATE]
        Ipp32s overheadBytes = 0;
        mfxBitstream* bs = mfxBs;
        mfxU32 initialDataLength = bs->DataLength;

        Ipp32s bs_main_id = m_videoParam.num_bs_subsets;
        Ipp8u *pbs0;
        Ipp32u bitOffset0;
        //Ipp32s overheadBytes0 = overheadBytes;
        H265Bs_GetState(&frameEnc->m_bs[bs_main_id], &pbs0, &bitOffset0);
        Ipp32u dataLength0 = mfxBs->DataLength;
        Ipp32s overheadBytes0 = overheadBytes;

        frameEnc->GetOutputData(bs, overheadBytes);

        // BRC
        if (m_brc) {
            const Ipp32s min_qp = 1;
            Ipp32s frameBytes = bs->DataLength - initialDataLength;
            mfxBRCStatus brcSts = m_brc->PostPackFrame(*frameEnc->GetVideoParam(),  task->m_sliceQpY, task->m_frameOrigin, frameBytes << 3, overheadBytes << 3, inputParam->m_reencode);
            inputParam->m_reencode = 0;

            if( brcSts != MFX_BRC_OK ) {
                if ((brcSts & MFX_BRC_ERR_SMALL_FRAME) && (task->m_sliceQpY < min_qp))
                    brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;
                if (brcSts == MFX_BRC_ERR_BIG_FRAME && task->m_sliceQpY == 51)
                    brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;

                if (!(brcSts & MFX_BRC_NOT_ENOUGH_BUFFER)) {
                    bs->DataLength = dataLength0;
                    H265Bs_SetState(&frameEnc->m_bs[bs_main_id], pbs0, bitOffset0);
                    overheadBytes = overheadBytes0;

                    // [1] wait completion of all running tasks
                    TaskIter tit = m_outputQueue.begin();
                    for( size_t taskIdx = 0; taskIdx < inputParam->m_outputQueueSize; taskIdx++ ) {
                        while( Ipp32u sum_of_elems =std::accumulate((*tit)->m_ithreadPool.begin(), (*tit)->m_ithreadPool.end(),0) > 0 ) {
                            thread_sleep(0);
                            //x86_pause();
                        }
                        if(taskIdx + 1 < inputParam->m_outputQueueSize) 
                            tit++;
                    }

                    // [2] re-init via BRC
                    // encodeQueue -> outputQueue
                    std::list<Task*> listQueue[] = {m_outputQueue, m_encodeQueue};
                    Ipp32s numCtb = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;
                    Ipp32s qIdxCnt = (m_videoParam.m_framesInParallel > 1) ? 2 : 1;

                    for( Ipp32s qIdx = 0; qIdx < qIdxCnt; qIdx++) {
                        std::list<Task*> & queue = listQueue[qIdx];
                        for( tit = queue.begin(); tit != queue.end(); tit++) {

                            (*tit)->m_sliceQpY = (Ipp8s)m_brc->GetQP(m_videoParam, (*tit)->m_frameOrigin);

                            memset(& (*tit)->m_lcuQps[0], (*tit)->m_sliceQpY, sizeof((*tit)->m_sliceQpY)*numCtb);

                            H265Slice *currSlices = (*tit)->m_slices;
                            for (Ipp8u i = 0; i < m_videoParam.NumSlices; i++) {
                                (currSlices + i)->slice_qp_delta = (*tit)->m_sliceQpY - m_pps.init_qp;
                                SetAllLambda(m_videoParam, (currSlices + i), (*tit)->m_sliceQpY, (*tit)->m_frameOrigin );
                            }
                        }
                    }

                    // [3] reset encoded data
                    tit = m_outputQueue.begin();
                    for( size_t taskIdx = 0; taskIdx < m_outputQueue.size(); taskIdx++ ) { // <- issue with Size()!!!

                        if( (*tit)->m_statusReport == 1 || taskIdx == 0 ) {
                            m_frameEncoder[ (*tit)->m_encIdx ]->SetEncodeTask( (*tit) );
                        }
                        if(taskIdx + 1 < m_outputQueue.size()) 
                            tit++;
                    }

                    vm_interlocked_cas32( &inputParam->m_reencode, 1, 0);
                    vm_interlocked_cas32( &(m_frameEncoder[task->m_encIdx]->GetEncodeTask()->m_statusReport), 7, 2); // signal to restart!!!

                    if (m_videoParam.num_threads > 1)
                        m_semaphore.Signal(m_videoParam.num_threads - 1); // wake up all threads
                    return; // repack!!!

                } else if (brcSts & MFX_BRC_ERR_SMALL_FRAME) {

                    Ipp32s maxSize, minSize, bitsize = frameBytes << 3;
                    Ipp8u *p = mfxBs->Data + mfxBs->DataOffset + mfxBs->DataLength;
                    m_brc->GetMinMaxFrameSize(&minSize, &maxSize);

                    if (minSize >  ((Ipp32s)mfxBs->MaxLength << 3))
                        minSize = (Ipp32s)mfxBs->MaxLength << 3;

                    while (bitsize < minSize - 32) {
                        *(Ipp32u*)p = 0;
                        p += 4;
                        bitsize += 32;
                    }
                    while (bitsize < minSize) {
                        *p = 0;
                        p++;
                        bitsize += 8;
                    }
                    m_brc->PostPackFrame(*frameEnc->GetVideoParam(),  task->m_sliceQpY, task->m_frameOrigin, bitsize, (overheadBytes << 3) + bitsize - (frameBytes << 3), 1);
                    mfxBs->DataLength += (bitsize >> 3) - frameBytes;

                } else {
                    //return MFX_ERR_NOT_ENOUGH_BUFFER;
                }
            }
        }

        // bs update on completion stage
        m_totalBits += (bs->DataLength - initialDataLength) * 8;
        vm_interlocked_cas32( &(task->m_statusReport), 3, 2);
        if (m_videoParam.num_threads > 1)
            m_semaphore.Signal(m_videoParam.num_threads - 1); // wake up all threads
    }
}


class OnExitHelperRoutine
{
public:
    OnExitHelperRoutine(volatile Ipp32u * arg) : m_arg(arg)
    {}
    ~OnExitHelperRoutine() 
    { 
        if(m_arg) {
            //printf("\n     exit <=== encOrder = %i thread %i \n", m_task->m_encOrder, ithread);fflush(stderr);
            vm_interlocked_dec32(reinterpret_cast<volatile Ipp32u *>(m_arg));
        }
    }

private:
    volatile Ipp32u * m_arg;
};

//single sthread only!!!
void MFXVideoENCODEH265::PrepareToEncode(void *pParam)
{
    H265EncodeTaskInputParams *inputParam = (H265EncodeTaskInputParams*)pParam;

    AcceptFrameHelper(inputParam->ctrl, NULL, inputParam->surface, inputParam->bs);
    vm_interlocked_cas32( &inputParam->m_doStage, 2, 1);

    if(NULL == inputParam->bs) {
        return;
    }

    Ipp32s stage = vm_interlocked_cas32( &inputParam->m_doStage, 3, 2);
    if(stage == 2) {

        while( m_outputQueue.size() < (size_t)m_videoParam.m_framesInParallel && !m_encodeQueue.empty() ) {
            int encIdx = 0;
            AddNewOutputTask(encIdx);
        }

        TaskIter it = m_outputQueue.begin();
        inputParam->m_targetTask    = (*it);
        inputParam->m_outputQueueSize = m_outputQueue.size();

#if defined (MFX_VA)
        if (m_videoParam.enableCmFlag) {
            ProcessFrameFEI(inputParam->m_targetTask);
        }
#endif
        vm_interlocked_cas32( &inputParam->m_doStage, 4, 3);

        m_core->INeedMoreThreadsInside(this);

        if (m_videoParam.num_threads > 1)
            m_semaphore.Signal(m_videoParam.num_threads - 1);
        
#if defined (MFX_VA)
        if (m_videoParam.enableCmFlag) {
            //-----
            Task* nextTask = NULL;
            if ( inputParam->m_outputQueueSize > 1 ) {
                TaskIter it = m_outputQueue.begin();
                it++;
                nextTask = (*it);
            } else if ( !m_encodeQueue.empty() ) {
                TaskIter it2 = m_encodeQueue.begin();
                nextTask = (*it2);
            }
            //----
            if (nextTask)
                ProcessFrameFEI_Next(nextTask);
        }
#endif // MFX_VA
    }
}


//!!! THREADING - MASTER FUNCTION!!!
mfxStatus MFXVideoENCODEH265::TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    H265ENC_UNREFERENCED_PARAMETER(threadNumber);
    H265ENC_UNREFERENCED_PARAMETER(callNumber);

    if (pState == NULL || pParam == NULL) {
        return MFX_ERR_NULL_PTR;
    }

    MFXVideoENCODEH265 *th = static_cast<MFXVideoENCODEH265 *>(pState);
    H265EncodeTaskInputParams *inputParam = (H265EncodeTaskInputParams*)pParam;
    mfxStatus sts = MFX_ERR_NONE;    

    // [single thread] :: prepare to encode (accept new frame, configuration, paq etc)
    Ipp32s stage = vm_interlocked_cas32( &inputParam->m_doStage, 1, 0);
    if (0 == stage) {
        th->PrepareToEncode(pParam);// here <m_doStage> will be switched ->2->3->4 consequentially

        if( th->m_videoParam.preEncMode ) {
            th->LookAheadAnalysis();
        }
    }

    // early termination if no external bs 
    if(NULL == inputParam->bs) {
        return MFX_TASK_DONE;
    }

    // threading barrier: if not completed preparation stage
    if(inputParam->m_doStage < 4) {
        th->m_semaphore.Wait();
    }

    // global thread count control
    Ipp32u newThreadCount = vm_interlocked_inc32( &inputParam->m_threadCount );
    OnExitHelperRoutine onExitHelper( &inputParam->m_threadCount );
    if (newThreadCount > th->m_videoParam.num_threads) {
        return MFX_TASK_BUSY;
    }

    // [multi threading]::FRAME THREADING LOOP
    Ipp32u failCounter = 0;
    while (inputParam->m_targetTask->m_statusReport < 3 || inputParam->m_targetTask->m_statusReport == 7) {

        // restart due to BRC repack
        Ipp32u stageReencode = vm_interlocked_cas32( &inputParam->m_targetTask->m_statusReport, 1, 7 );
        if (stageReencode == 1 ) {
            //    something to do
        }

        TaskIter beg = th->m_outputQueue.begin();
        for (size_t taskIdx = 0; taskIdx < inputParam->m_outputQueueSize; taskIdx++) {
            Task* nextTask = *beg;
            if(nextTask->m_statusReport == 1 ) {
                sts = th->EncSolver(nextTask, (taskIdx == 0) ? NULL : &(inputParam->m_targetTask->m_statusReport)); // taskIdx == 0 the same as nextTask == inputParam->targetTask
                if (sts == 100) {
                    failCounter++;
                }
                else {
                    failCounter = 0;
                }
                if(taskIdx == 0 && sts == MFX_TASK_DONE) { // special case OnComplete for expectedTask
                    th->SyncOnTaskCompletion(inputParam->m_targetTask, inputParam->bs, inputParam);
                }
                if(sts == MFX_TASK_DONE || sts == MFX_TASK_WORKING) {
                    break;
                }
            }
            if (taskIdx + 1 < inputParam->m_outputQueueSize) {
                beg++;
            }
        }

        if (failCounter >= inputParam->m_outputQueueSize) {
            th->m_semaphore.Wait();
            failCounter = 0;
        }
    }

    // extra stage on TaskCompletion. fake.
    if (Ipp32u stageOnExit = vm_interlocked_cas32( &(inputParam->m_targetTask->m_statusReport), 4, 3) == 3) {
        return MFX_TASK_DONE;
    }

    return MFX_TASK_DONE;
} 


mfxStatus MFXVideoENCODEH265::TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes)
{
    H265ENC_UNREFERENCED_PARAMETER(taskRes);
    MFX_CHECK_NULL_PTR1(pState);
    MFX_CHECK_NULL_PTR1(pParam);

    MFXVideoENCODEH265 *th = static_cast<MFXVideoENCODEH265 *>(pState);
    H265EncodeTaskInputParams *inputParam = static_cast<H265EncodeTaskInputParams*>(pParam);
    mfxBitstream* bs = inputParam->bs;

    if (bs) {
        Task* codedTask = inputParam->m_targetTask;
        H265FrameEncoder* frameEnc = th->m_frameEncoder[codedTask->m_encIdx];

        // STAGE::[Resource Release]
        {
            while (th->m_semaphore.TryWait() == VM_OK); // reset semaphore
            th->OnEncodingQueried( codedTask ); // remove codedTask from encodeQueue (outputQueue) and clean (release) dpb.
            frameEnc->SetFree(true);
        }
    }

    H265_Free(pParam);

    return MFX_TASK_DONE;

} // 


void MFXVideoENCODEH265::ProcessFrameFEI(Task* task)
{
    task;
#if defined(MFX_VA)
    m_FeiCtx->ProcessFrameFEI(m_FeiCtx->feiInIdx, task->m_frameOrigin, task->m_slices, task->m_dpb, task->m_dpbSize, 1);
#endif

}


void MFXVideoENCODEH265::ProcessFrameFEI_Next(Task* task)
{
    task;
#if defined(MFX_VA)
    m_FeiCtx->ProcessFrameFEI(1 - m_FeiCtx->feiInIdx, task->m_frameOrigin, task->m_slices, task->m_dpb, task->m_dpbSize, 0);
    m_FeiCtx->feiInIdx = 1 - m_FeiCtx->feiInIdx;
#endif
}


mfxFrameSurface1* MFXVideoENCODEH265::GetOriginalSurface(mfxFrameSurface1* input)
{
    if (m_isOpaque) {
        mfxFrameSurface1* out = m_core->GetNativeSurface(input);
        if (out != input) { // input surface is opaque surface
            out->Data.FrameOrder = input->Data.FrameOrder;
            out->Data.TimeStamp = input->Data.TimeStamp;
            out->Data.Corrupted = input->Data.Corrupted;
            out->Data.DataFlag = input->Data.DataFlag;
            out->Info = input->Info;
        }
        return out;
    }

    return input;
}


H265Frame* MFXVideoENCODEH265::InsertInputFrame(const mfxFrameSurface1 *surface)
{
    // get free task
    if( m_free.empty() ) {
        H265Enc::Task* task = new H265Enc::Task;
        task->Create(m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs, m_videoParam.num_thread_structs);
        m_free.push_back(task);
    }

    m_free.front()->Reset();
    m_inputQueue.splice(m_inputQueue.end(), m_free, m_free.begin());


    FramePtrIter frm = GetFreeFrame(m_freeFrames, &m_videoParam);
    if(frm == m_freeFrames.end()) return NULL;

    // light configure
    (*frm)->ResetEncInfo();
    (*frm)->CopyFrame(surface);
    (*frm)->m_timeStamp = surface->Data.TimeStamp;
    m_inputQueue.back()->m_frameOrigin = *frm;

    return m_inputQueue.back()->m_frameOrigin;

} // 

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
