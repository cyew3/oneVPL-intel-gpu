//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <math.h>
#include <algorithm>

#include "ippi.h"
#include "vm_interlocked.h"
#include "vm_sys_info.h"

#include "mfx_h265_enc.h"
#include "mfx_h265_brc.h"

#include "mfx_h265_paq.h"

#ifdef MFX_ENABLE_WATERMARK
#include "watermark.h"
#endif

#include "mfx_h265_enc_cm_defs.h"

namespace H265Enc {

extern Ipp32s cmCurIdx;
extern Ipp32s cmNextIdx;

#if defined( _WIN32) || defined(_WIN64)
#define thread_sleep(nms) Sleep(nms)
#else
#define thread_sleep(nms)
#endif
#define x86_pause() _mm_pause()


//[strength][isNotI][log2w-3][2]
Ipp32f h265_split_thresholds_cu_1[3][2][4][2] = {{{{0, 0},{6.068657e+000, 1.955850e-001},{2.794428e+001, 1.643400e-001},{4.697359e+002, 1.113505e-001},},
{{0, 0},{8.335527e+000, 2.312656e-001},{1.282028e+001, 2.317006e-001},{3.218380e+001, 2.157863e-001},},
},{{{0, 0},{1.412660e+001, 1.845380e-001},{1.967989e+001, 1.873910e-001},{3.617037e+002, 1.257173e-001},},
{{0, 0},{2.319860e+001, 2.194218e-001},{2.436671e+001, 2.260317e-001},{3.785467e+001, 2.252255e-001},},
},{{{0, 0},{1.465458e+001, 1.946118e-001},{1.725847e+001, 1.969719e-001},{3.298872e+002, 1.336427e-001},},
{{0, 0},{3.133788e+001, 2.192709e-001},{3.292004e+001, 2.266248e-001},{3.572242e+001, 2.357623e-001},},
},};

//[strength][isNotI][log2w-3][2]
Ipp32f h265_split_thresholds_tu_1[3][2][4][2] = {{{{9.861009e+001, 2.014005e-001},{1.472963e+000, 2.407408e-001},{7.306935e+000, 2.073082e-001},{0, 0},},
{{1.227516e+002, 1.973595e-001},{1.617983e+000, 2.342001e-001},{6.189427e+000, 2.090359e-001},{0, 0},},
},{{{9.861009e+001, 2.014005e-001},{1.343445e+000, 2.666583e-001},{4.266056e+000, 2.504071e-001},{0, 0},},
{{1.227516e+002, 1.973595e-001},{2.443985e+000, 2.427829e-001},{4.778531e+000, 2.343920e-001},{0, 0},},
},{{{9.861009e+001, 2.014005e-001},{1.530746e+000, 3.027675e-001},{1.222733e+001, 2.649870e-001},{0, 0},},
{{1.227516e+002, 1.973595e-001},{2.158767e+000, 2.696284e-001},{3.401860e+000, 2.652995e-001},{0, 0},},
},};

//[strength][isNotI][log2w-3][2]
Ipp32f h265_split_thresholds_cu_4[3][2][4][2] = {{{{0, 0},{4.218807e+000, 2.066010e-001},{2.560569e+001, 1.677201e-001},{0, 0},},
{{0, 0},{4.848787e+000, 2.299086e-001},{1.198510e+001, 2.170575e-001},{0, 0},},
},{{{0, 0},{5.227898e+000, 2.117919e-001},{1.819586e+001, 1.910210e-001},{0, 0},},
{{0, 0},{1.098668e+001, 2.226733e-001},{2.273784e+001, 2.157968e-001},{0, 0},},
},{{{0, 0},{4.591922e+000, 2.238883e-001},{1.480393e+001, 2.032860e-001},{0, 0},},
{{0, 0},{1.831528e+001, 2.201327e-001},{2.999500e+001, 2.175826e-001},{0, 0},},
},};

//[strength][isNotI][log2w-3][2]
Ipp32f h265_split_thresholds_tu_4[3][2][4][2] = {{{{1.008684e+002, 2.003675e-001},{2.278697e+000, 2.226109e-001},{1.767972e+001, 1.733893e-001},{0, 0},},
{{1.281323e+002, 1.984014e-001},{2.906725e+000, 2.063635e-001},{1.106598e+001, 1.798757e-001},{0, 0},},
},{{{1.008684e+002, 2.003675e-001},{2.259367e+000, 2.473857e-001},{3.871648e+001, 1.789534e-001},{0, 0},},
{{1.281323e+002, 1.984014e-001},{4.119740e+000, 2.092187e-001},{9.931154e+000, 1.907941e-001},{0, 0},},
},{{{1.008684e+002, 2.003675e-001},{1.161667e+000, 3.112699e-001},{5.179975e+001, 2.364036e-001},{0, 0},},
{{1.281323e+002, 1.984014e-001},{4.326205e+000, 2.262409e-001},{2.088369e+001, 1.829562e-001},{0, 0},},
},};

//[strength][isNotI][log2w-3][2]
Ipp32f h265_split_thresholds_cu_7[3][2][4][2] = {{{{0, 0},{0, 0},{2.868749e+001, 1.759965e-001},{0, 0},},
{{0, 0},{0, 0},{1.203077e+001, 2.066983e-001},{0, 0},},
},{{{0, 0},{0, 0},{2.689497e+001, 1.914970e-001},{0, 0},},
{{0, 0},{0, 0},{1.856055e+001, 2.129173e-001},{0, 0},},
},{{{0, 0},{0, 0},{2.268480e+001, 2.049253e-001},{0, 0},},
{{0, 0},{0, 0},{3.414455e+001, 2.067140e-001},{0, 0},},
},};

//[strength][isNotI][log2w-3][2]
Ipp32f h265_split_thresholds_tu_7[3][2][4][2] = {{{{7.228049e+001, 1.900956e-001},{2.365207e+000, 2.211628e-001},{4.651502e+001, 1.425675e-001},{0, 0},},
{{0, 0},{2.456053e+000, 2.100343e-001},{7.537189e+000, 1.888014e-001},{0, 0},},
},{{{7.228049e+001, 1.900956e-001},{1.293689e+000, 2.672417e-001},{1.633606e+001, 2.076887e-001},{0, 0},},
{{0, 0},{1.682124e+000, 2.368017e-001},{5.516403e+000, 2.080632e-001},{0, 0},},
},{{{7.228049e+001, 1.900956e-001},{1.479032e+000, 2.785342e-001},{8.723769e+000, 2.469559e-001},{0, 0},},
{{0, 0},{1.943537e+000, 2.548912e-001},{1.756701e+001, 1.900659e-001},{0, 0},},
},};


typedef Ipp32f thres_tab[2][4][2];

static Ipp64f h265_calc_split_threshold(Ipp32s TU, Ipp32s isNotCu, Ipp32s isNotI, Ipp32s log2width, Ipp32s strength, Ipp32s QP)
{
    thres_tab *h265_split_thresholds_tab;

    switch(TU) {
    case 1:
    case 2:
    case 3:
        h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_1 : h265_split_thresholds_cu_1;
        break;
    default:
    case 0:
    case 4:
    case 5:
    case 6:
        h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_4 : h265_split_thresholds_cu_4;
        break;
    case 7:
        h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_7 : h265_split_thresholds_cu_7;
        break;
    }

    if (strength == 0)
        return 0;
    if (strength > 3)
        strength = 3;

    if (isNotCu) {
        if ((log2width < 3 || log2width > 5)) {
            //VM_ASSERT(0);
            return 0;
        }
    }
    else {
        if ((log2width < 4 || log2width > 6)) {
            //VM_ASSERT(0);
            return 0;
        }
    }

    double a = h265_split_thresholds_tab[strength - 1][isNotI][log2width - 3][0];
    double b = h265_split_thresholds_tab[strength - 1][isNotI][log2width - 3][1];
    return a * exp(b * QP);
}


static const mfxU16 tab_AspectRatio265[17][2] =
{
    { 1,  1},  // unspecified
    { 1,  1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11},
    {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160,99}, { 4,  3}, { 3,  2}, { 2,  1}
};

static mfxU32 ConvertSARtoIDC_H265enc(mfxU32 sarw, mfxU32 sarh)
{
    for (mfxU32 i = 1; i < sizeof(tab_AspectRatio265) / sizeof(tab_AspectRatio265[0]); i++)
        if (sarw * tab_AspectRatio265[i][1] == sarh * tab_AspectRatio265[i][0])
            return i;
    return 0;
}

mfxStatus H265Encoder::InitH265VideoParam(const mfxVideoParam *param, const mfxExtCodingOptionHEVC *optsHevc)
{
    H265VideoParam *pars = &m_videoParam;
    Ipp32u width = param->mfx.FrameInfo.Width;
    Ipp32u height = param->mfx.FrameInfo.Height;

    pars->bitDepthLuma = 8;
    pars->bitDepthChroma = 8;

    pars->SourceWidth = width;
    pars->SourceHeight = height;
    pars->Log2MaxCUSize = optsHevc->Log2MaxCUSize;// 6;
    pars->MaxCUDepth = optsHevc->MaxCUDepth; // 4;
    pars->QuadtreeTULog2MaxSize = optsHevc->QuadtreeTULog2MaxSize; // 5;
    pars->QuadtreeTULog2MinSize = optsHevc->QuadtreeTULog2MinSize; // 2;
    pars->QuadtreeTUMaxDepthIntra = optsHevc->QuadtreeTUMaxDepthIntra; // 4;
    pars->QuadtreeTUMaxDepthInter = optsHevc->QuadtreeTUMaxDepthInter; // 4;
    pars->partModes = optsHevc->PartModes;
    pars->TMVPFlag = (optsHevc->TMVP == MFX_CODINGOPTION_ON);
    pars->QPI = (Ipp8s)param->mfx.QPI;
    pars->QPP = (Ipp8s)param->mfx.QPP;
    pars->QPB = (Ipp8s)param->mfx.QPB;
    pars->NumSlices = param->mfx.NumSlice;

    pars->GopPicSize = param->mfx.GopPicSize;
    pars->GopRefDist = param->mfx.GopRefDist;
    pars->IdrInterval = param->mfx.IdrInterval;
    pars->GopClosedFlag = (param->mfx.GopOptFlag & MFX_GOP_CLOSED) != 0;
    pars->GopStrictFlag = (param->mfx.GopOptFlag & MFX_GOP_STRICT) != 0;
    pars->BPyramid = pars->GopRefDist > 2 && optsHevc->BPyramid == MFX_CODINGOPTION_ON;

    if (!pars->GopRefDist) /*|| (pars->GopPicSize % pars->GopRefDist)*/
        return MFX_ERR_INVALID_VIDEO_PARAM;

    pars->NumRefToStartCodeBSlice = 1;
    pars->TreatBAsReference = 0;

    if (pars->BPyramid) {
        pars->MaxRefIdxL0 = (Ipp8u)param->mfx.NumRefFrame;
        pars->MaxRefIdxL1 = (Ipp8u)param->mfx.NumRefFrame;
    }
    else {
        pars->MaxRefIdxL0 = (Ipp8u)param->mfx.NumRefFrame;
        pars->MaxRefIdxL1 = 1;
    }
    pars->MaxBRefIdxL0 = 1;
    pars->GeneralizedPB = (optsHevc->GPB == MFX_CODINGOPTION_ON);
    pars->PGopPicSize = (pars->GopRefDist > 1 || pars->MaxRefIdxL0 == 1) ? 1 : PGOP_PIC_SIZE;
    pars->NumRefFrames = MAX(6, pars->MaxRefIdxL0);

    pars->AnalyseFlags = 0;
    if (optsHevc->AnalyzeChroma == MFX_CODINGOPTION_ON)
        pars->AnalyseFlags |= HEVC_ANALYSE_CHROMA;
    if (optsHevc->CostChroma == MFX_CODINGOPTION_ON)
        pars->AnalyseFlags |= HEVC_COST_CHROMA;

    pars->SplitThresholdStrengthCUIntra = (Ipp8u)optsHevc->SplitThresholdStrengthCUIntra - 1;
    pars->SplitThresholdStrengthTUIntra = (Ipp8u)optsHevc->SplitThresholdStrengthTUIntra - 1;
    pars->SplitThresholdStrengthCUInter = (Ipp8u)optsHevc->SplitThresholdStrengthCUInter - 1;

    pars->SBHFlag  = (optsHevc->SignBitHiding == MFX_CODINGOPTION_ON);
    pars->RDOQFlag = (optsHevc->RDOQuant == MFX_CODINGOPTION_ON);
    pars->rdoqChromaFlag = (optsHevc->RDOQuantChroma == MFX_CODINGOPTION_ON);
    pars->rdoqCGZFlag = (optsHevc->RDOQuantCGZ == MFX_CODINGOPTION_ON);
    pars->SAOFlag  = (optsHevc->SAO == MFX_CODINGOPTION_ON);
    pars->WPPFlag  = (optsHevc->WPP == MFX_CODINGOPTION_ON) || (optsHevc->WPP == MFX_CODINGOPTION_UNKNOWN && param->mfx.NumThread > 1);
    if (pars->WPPFlag) {
        pars->num_threads = param->mfx.NumThread;
        if (pars->num_threads == 0)
            pars->num_threads = vm_sys_info_get_cpu_num();
        if (pars->num_threads < 1) {
            pars->num_threads = 1;
        }
    }
    else {
        pars->num_threads = 1;
    }

    for (Ipp32s i = 0; i <= 6; i++) {
        Ipp32s w = (1 << i);
        pars->num_cand_0[i] = 33;
        pars->num_cand_1[i] = w > 8 ? 4 : 8;
        pars->num_cand_2[i] = pars->num_cand_1[i] >> 1;
    }
    if (optsHevc->IntraNumCand0_2) pars->num_cand_0[2] = (Ipp8u)optsHevc->IntraNumCand0_2;
    if (optsHevc->IntraNumCand0_3) pars->num_cand_0[3] = (Ipp8u)optsHevc->IntraNumCand0_3;
    if (optsHevc->IntraNumCand0_4) pars->num_cand_0[4] = (Ipp8u)optsHevc->IntraNumCand0_4;
    if (optsHevc->IntraNumCand0_5) pars->num_cand_0[5] = (Ipp8u)optsHevc->IntraNumCand0_5;
    if (optsHevc->IntraNumCand0_6) pars->num_cand_0[6] = (Ipp8u)optsHevc->IntraNumCand0_6;
    if (optsHevc->IntraNumCand1_2) pars->num_cand_1[2] = (Ipp8u)optsHevc->IntraNumCand1_2;
    if (optsHevc->IntraNumCand1_3) pars->num_cand_1[3] = (Ipp8u)optsHevc->IntraNumCand1_3;
    if (optsHevc->IntraNumCand1_4) pars->num_cand_1[4] = (Ipp8u)optsHevc->IntraNumCand1_4;
    if (optsHevc->IntraNumCand1_5) pars->num_cand_1[5] = (Ipp8u)optsHevc->IntraNumCand1_5;
    if (optsHevc->IntraNumCand1_6) pars->num_cand_1[6] = (Ipp8u)optsHevc->IntraNumCand1_6;
    if (optsHevc->IntraNumCand2_2) pars->num_cand_2[2] = (Ipp8u)optsHevc->IntraNumCand2_2;
    if (optsHevc->IntraNumCand2_3) pars->num_cand_2[3] = (Ipp8u)optsHevc->IntraNumCand2_3;
    if (optsHevc->IntraNumCand2_4) pars->num_cand_2[4] = (Ipp8u)optsHevc->IntraNumCand2_4;
    if (optsHevc->IntraNumCand2_5) pars->num_cand_2[5] = (Ipp8u)optsHevc->IntraNumCand2_5;
    if (optsHevc->IntraNumCand2_6) pars->num_cand_2[6] = (Ipp8u)optsHevc->IntraNumCand2_6;

//kolya HM's number of intra modes to search through
#define HM_MATCH_2 0
#if HM_MATCH_2
    pars->num_cand_1[1] = 3; // 2x2
    pars->num_cand_1[2] = 8; // 4x4
    pars->num_cand_1[3] = 8; // 8x8
    pars->num_cand_1[4] = 3; // 16x16
    pars->num_cand_1[5] = 3; // 32x32
    pars->num_cand_1[6] = 3; // 64x64
    pars->num_cand_1[7] = 3; // 128x128
#endif

#if defined (MFX_ENABLE_CM)
    pars->enableCmFlag = (optsHevc->EnableCm == MFX_CODINGOPTION_ON);
#else
    pars->enableCmFlag = 0;
#endif // MFX_ENABLE_CM

    pars->preEncMode = 0;
#if defined(MFX_ENABLE_H265_PAQ)
    // support for 64x64 && BPyramid && RefDist==8 (aka tu1 & tu2)
    if( pars->BPyramid && (8 == pars->GopRefDist) && (6 == pars->Log2MaxCUSize) )
        pars->preEncMode = optsHevc->DQP;
#endif

    pars->cmIntraThreshold = optsHevc->CmIntraThreshold;
    pars->tuSplitIntra = optsHevc->TUSplitIntra;
    pars->cuSplit = optsHevc->CUSplit;
    pars->intraAngModes = optsHevc->IntraAngModes;
    pars->fastSkip = (optsHevc->FastSkip == MFX_CODINGOPTION_ON);
    pars->fastCbfMode = (optsHevc->FastCbfMode == MFX_CODINGOPTION_ON);
    pars->hadamardMe = optsHevc->HadamardMe;
    pars->TMVPFlag = (optsHevc->TMVP == MFX_CODINGOPTION_ON);
    pars->deblockingFlag = (optsHevc->Deblocking == MFX_CODINGOPTION_ON);
    pars->saoOpt = optsHevc->SaoOpt;
    pars->patternIntPel = optsHevc->PatternIntPel;
    pars->patternSubPel = optsHevc->PatternSubPel;
    pars->numBiRefineIter = optsHevc->NumBiRefineIter;
    pars->puDecisionSatd = (optsHevc->PuDecisionSatd == MFX_CODINGOPTION_ON);
    pars->minCUDepthAdapt = (optsHevc->MinCUDepthAdapt == MFX_CODINGOPTION_ON);
    pars->cuSplitThreshold = optsHevc->CUSplitThreshold;

    for (Ipp32s i = 0; i <= 6; i++) {
        pars->num_cand_0[i] = Saturate(1, 33, pars->num_cand_0[i]);
        pars->num_cand_1[i] = Saturate(1, pars->num_cand_0[i] + 2, pars->num_cand_1[i]);
        pars->num_cand_2[i] = Saturate(1, pars->num_cand_1[i], pars->num_cand_2[i]);
    }

    /* derived */

    pars->MaxTrSize = 1 << pars->QuadtreeTULog2MaxSize;
    pars->MaxCUSize = 1 << pars->Log2MaxCUSize;

    pars->AddCUDepth  = 0;
    while ((pars->MaxCUSize>>pars->MaxCUDepth) >
        (1u << ( pars->QuadtreeTULog2MinSize + pars->AddCUDepth)))
    {
        pars->AddCUDepth++;
    }

    pars->MaxCUDepth += pars->AddCUDepth;
    pars->AddCUDepth++;

    pars->MinCUSize = pars->MaxCUSize >> (pars->MaxCUDepth - pars->AddCUDepth);
    pars->MinTUSize = pars->MaxCUSize >> pars->MaxCUDepth;

    if (pars->MinTUSize != 1u << pars->QuadtreeTULog2MinSize)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    pars->CropLeft = param->mfx.FrameInfo.CropX;
    pars->CropTop = param->mfx.FrameInfo.CropY;
    pars->CropRight = param->mfx.FrameInfo.CropW ? param->mfx.FrameInfo.Width - param->mfx.FrameInfo.CropW - param->mfx.FrameInfo.CropX : 0;
    pars->CropBottom = param->mfx.FrameInfo.CropH ? param->mfx.FrameInfo.Height - param->mfx.FrameInfo.CropH - param->mfx.FrameInfo.CropY : 0;

    pars->Width = width;
    pars->Height = height;

    pars->Log2NumPartInCU = pars->MaxCUDepth << 1;
    pars->NumPartInCU = 1 << pars->Log2NumPartInCU;
    pars->NumPartInCUSize  = 1 << pars->MaxCUDepth;

    pars->PicWidthInMinCbs = pars->Width / pars->MinCUSize;
    pars->PicHeightInMinCbs = pars->Height / pars->MinCUSize;
    pars->PicWidthInCtbs = (pars->Width + pars->MaxCUSize - 1) / pars->MaxCUSize;
    pars->PicHeightInCtbs = (pars->Height + pars->MaxCUSize - 1) / pars->MaxCUSize;

    if (pars->num_threads > pars->PicHeightInCtbs)
        pars->num_threads = pars->PicHeightInCtbs;
    if (pars->num_threads > (pars->PicWidthInCtbs + 1) / 2)
        pars->num_threads = (pars->PicWidthInCtbs + 1) / 2;

    pars->threading_by_rows = pars->WPPFlag && pars->num_threads > 1 && pars->NumSlices == 1 ? 0 : 1;
    pars->num_thread_structs = pars->threading_by_rows ? pars->num_threads : pars->PicHeightInCtbs;

    for (Ipp32s i = 0; i < pars->MaxCUDepth; i++ )
        pars->AMPAcc[i] = i < pars->MaxCUDepth-pars->AddCUDepth ? (pars->partModes==3) : 0;

    // deltaQP control, main control params
    pars->MaxCuDQPDepth = 0;
    pars->m_maxDeltaQP = 0;
    pars->UseDQP = 0;
    
    if(pars->preEncMode)
    {
        pars->MaxCuDQPDepth = 0;
        pars->m_maxDeltaQP = 0;
        pars->UseDQP = 1;
    }
    if (pars->MaxCuDQPDepth > 0 || pars->m_maxDeltaQP > 0)
        pars->UseDQP = 1;
    
    if (pars->UseDQP)
        pars->MinCuDQPSize = pars->MaxCUSize >> pars->MaxCuDQPDepth;
    else
        pars->MinCuDQPSize = pars->MaxCUSize;
    
    pars->NumMinTUInMaxCU = pars->MaxCUSize >> pars->QuadtreeTULog2MinSize;
    //pars->Log2MinTUSize = pars->QuadtreeTULog2MinSize;
    //pars->MaxTotalDepth = pars->MaxCUDepth;

    pars->FrameRateExtN = param->mfx.FrameInfo.FrameRateExtN;
    pars->FrameRateExtD = param->mfx.FrameInfo.FrameRateExtD;
    pars->AspectRatioW  = param->mfx.FrameInfo.AspectRatioW ;
    pars->AspectRatioH  = param->mfx.FrameInfo.AspectRatioH ;

    pars->Profile = param->mfx.CodecProfile;
    pars->Tier = (param->mfx.CodecLevel & MFX_TIER_HEVC_HIGH) ? 1 : 0;
    pars->Level = (param->mfx.CodecLevel &~ MFX_TIER_HEVC_HIGH) * 1; // mult 3 it SetProfileLevel

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetVPS()
{
    H265VidParameterSet *vps = &m_vps;

    memset(vps, 0, sizeof(H265VidParameterSet));
    vps->vps_max_layers = 1;
    vps->vps_max_sub_layers = 1;
    vps->vps_temporal_id_nesting_flag = 1;
    if (m_videoParam.BPyramid)
        vps->vps_max_dec_pic_buffering[0] = m_videoParam.GopRefDist == 8 ? 4 : 3;
    else
        vps->vps_max_dec_pic_buffering[0] = (Ipp8u)(MAX(m_videoParam.MaxRefIdxL0,m_videoParam.MaxBRefIdxL0) +
            m_videoParam.MaxRefIdxL1);

    vps->vps_max_num_reorder_pics[0] = 0;
    if (m_videoParam.GopRefDist > 1) {
        vps->vps_max_num_reorder_pics[0] = (m_videoParam.BPyramid)
            ? H265_CeilLog2(m_videoParam.GopRefDist)
            : 1;
    }

    vps->vps_max_latency_increase[0] = 0;
    vps->vps_num_layer_sets = 1;

    if (m_videoParam.FrameRateExtD && m_videoParam.FrameRateExtN) {
        vps->vps_timing_info_present_flag = 1;
        vps->vps_num_units_in_tick = m_videoParam.FrameRateExtD / 1;
        vps->vps_time_scale = m_videoParam.FrameRateExtN;
        vps->vps_poc_proportional_to_timing_flag = 0;
        vps->vps_num_hrd_parameters = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetProfileLevel()
{
    memset(&m_profile_level, 0, sizeof(H265ProfileLevelSet));
    m_profile_level.general_profile_idc = (mfxU8) m_videoParam.Profile; //MFX_PROFILE_HEVC_MAIN;
    m_profile_level.general_tier_flag = (mfxU8) m_videoParam.Tier;
    m_profile_level.general_level_idc = (mfxU8) (m_videoParam.Level * 3);
    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetSPS()
{
    H265SeqParameterSet *sps = &m_sps;
    H265VidParameterSet *vps = &m_vps;
    H265VideoParam *pars = &m_videoParam;

    memset(sps, 0, sizeof(H265SeqParameterSet));

    sps->sps_video_parameter_set_id = vps->vps_video_parameter_set_id;
    sps->sps_max_sub_layers = 1;
    sps->sps_temporal_id_nesting_flag = 1;

    sps->chroma_format_idc = 1;

    sps->pic_width_in_luma_samples = pars->Width;
    sps->pic_height_in_luma_samples = pars->Height;
    sps->bit_depth_luma = pars->bitDepthLuma;
    sps->bit_depth_chroma = pars->bitDepthChroma;

    if (pars->CropLeft | pars->CropTop | pars->CropRight | pars->CropBottom) {
        sps->conformance_window_flag = 1;
        sps->conf_win_left_offset = pars->CropLeft / 2;
        sps->conf_win_top_offset = pars->CropTop / 2;
        sps->conf_win_right_offset = pars->CropRight / 2;
        sps->conf_win_bottom_offset = pars->CropBottom / 2;
    }

    sps->log2_diff_max_min_coding_block_size = (Ipp8u)(pars->MaxCUDepth - pars->AddCUDepth);
    sps->log2_min_coding_block_size_minus3 = (Ipp8u)(pars->Log2MaxCUSize - sps->log2_diff_max_min_coding_block_size - 3);

    sps->log2_min_transform_block_size_minus2 = (Ipp8u)(pars->QuadtreeTULog2MinSize - 2);
    sps->log2_diff_max_min_transform_block_size = (Ipp8u)(pars->QuadtreeTULog2MaxSize - pars->QuadtreeTULog2MinSize);

    sps->max_transform_hierarchy_depth_intra = (Ipp8u)pars->QuadtreeTUMaxDepthIntra;
    sps->max_transform_hierarchy_depth_inter = (Ipp8u)pars->QuadtreeTUMaxDepthInter;

    sps->amp_enabled_flag = (pars->partModes==3);
    sps->sps_temporal_mvp_enabled_flag = pars->TMVPFlag;
    sps->sample_adaptive_offset_enabled_flag = pars->SAOFlag;
    sps->strong_intra_smoothing_enabled_flag = 1;

    InitShortTermRefPicSet();

    if (m_videoParam.GopRefDist == 1)
    {
        sps->log2_max_pic_order_cnt_lsb = 4;
    } else {
        Ipp32s log2_max_poc = H265_CeilLog2(m_videoParam.GopRefDist - 1 + m_videoParam.NumRefFrames) + 3;

        sps->log2_max_pic_order_cnt_lsb = (Ipp8u)MAX(log2_max_poc, 4);

        if (sps->log2_max_pic_order_cnt_lsb > 16)
        {
            VM_ASSERT(false);
            sps->log2_max_pic_order_cnt_lsb = 16;
        }
    }
    sps->num_ref_frames = m_videoParam.NumRefFrames;

    sps->sps_num_reorder_pics[0] = vps->vps_max_num_reorder_pics[0];
    sps->sps_max_dec_pic_buffering[0] = vps->vps_max_dec_pic_buffering[0];
    sps->sps_max_latency_increase[0] = vps->vps_max_latency_increase[0];

    //VUI
    if (pars->AspectRatioW && pars->AspectRatioH) {
        sps->aspect_ratio_idc = (Ipp8u)ConvertSARtoIDC_H265enc(pars->AspectRatioW, pars->AspectRatioH);
        if (sps->aspect_ratio_idc == 0) {
            sps->aspect_ratio_idc = 255; //Extended SAR
            sps->sar_width = pars->AspectRatioW;
            sps->sar_height = pars->AspectRatioH;
        }
        sps->aspect_ratio_info_present_flag = 1;
    }
    if (sps->aspect_ratio_info_present_flag)
        sps->vui_parameters_present_flag = 1;


    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetPPS()
{
    H265SeqParameterSet *sps = &m_sps;
    H265PicParameterSet *pps = &m_pps;

    memset(pps, 0, sizeof(H265PicParameterSet));

    pps->pps_seq_parameter_set_id = sps->sps_seq_parameter_set_id;
    pps->deblocking_filter_control_present_flag = !m_videoParam.deblockingFlag;
    pps->deblocking_filter_override_enabled_flag = 0;
    pps->pps_deblocking_filter_disabled_flag = !m_videoParam.deblockingFlag;
    pps->pps_tc_offset_div2 = 0;
    pps->pps_beta_offset_div2 = 0;
    pps->pps_loop_filter_across_slices_enabled_flag = 1;
    pps->loop_filter_across_tiles_enabled_flag = 1;
    pps->entropy_coding_sync_enabled_flag = m_videoParam.WPPFlag;

    if (m_brc)
        pps->init_qp = (Ipp8s)m_brc->GetQP(MFX_FRAMETYPE_I);
    else
        pps->init_qp = m_videoParam.QPI;

    pps->log2_parallel_merge_level = 2;

    pps->num_ref_idx_l0_default_active = m_videoParam.MaxRefIdxL0;
    pps->num_ref_idx_l1_default_active = m_videoParam.MaxRefIdxL1;

    pps->sign_data_hiding_enabled_flag = m_videoParam.SBHFlag;
    
    if (m_videoParam.UseDQP) {
        pps->cu_qp_delta_enabled_flag = 1;
        pps->diff_cu_qp_delta_depth   = m_videoParam.MaxCuDQPDepth;
    }
    return MFX_ERR_NONE;
}

static Ipp32f tab_rdLambdaBPyramid[4] = {0.442f, 0.3536f, 0.3536f, 0.68f};

static Ipp32f tab_rdLambdaBPyramid_LoCmplx[4]  = {0.442f, 0.3536f, 0.4f, 0.68f};
static Ipp32f tab_rdLambdaBPyramid_MidCmplx[4] = {0.442f, 0.3536f, 0.3817, 0.6f};
static Ipp32f tab_rdLambdaBPyramid_HiCmplx[4]  = {0.442f, 0.2793, 0.3536, 0.5f};

mfxStatus H265Encoder::SetSlice(H265Slice *slice, Ipp32u curr_slice)
{
    memset(slice, 0, sizeof(H265Slice));

    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    Ipp32u numSlices = m_videoParam.NumSlices;
    Ipp32u slice_len = numCtbs / numSlices;

    slice->slice_segment_address = slice_len * curr_slice;
    slice->slice_address_last_ctb = slice_len * (curr_slice + 1) - 1;
    if (slice->slice_address_last_ctb > numCtbs - 1 || curr_slice == numSlices - 1)
        slice->slice_address_last_ctb = numCtbs - 1;

    if (curr_slice) slice->first_slice_segment_in_pic_flag = 0;
    else slice->first_slice_segment_in_pic_flag = 1;

    slice->slice_pic_parameter_set_id = m_pps.pps_pic_parameter_set_id;

    switch (m_pCurrentFrame->m_PicCodType) {
        case MFX_FRAMETYPE_P:
            slice->slice_type = P_SLICE; break;
        case MFX_FRAMETYPE_B:
            slice->slice_type = B_SLICE; break;
        case MFX_FRAMETYPE_I:
        default:
            slice->slice_type = I_SLICE; break;
    }

    if (m_pCurrentFrame->m_bIsIDRPic)
    {
        slice->IdrPicFlag = 1;
        slice->RapPicFlag = 1;
    }

    slice->slice_pic_order_cnt_lsb =  m_pCurrentFrame->PicOrderCnt() & ~(0xffffffff << m_sps.log2_max_pic_order_cnt_lsb);
    slice->deblocking_filter_override_flag = 0;
    slice->slice_deblocking_filter_disabled_flag = m_pps.pps_deblocking_filter_disabled_flag;
    slice->slice_tc_offset_div2 = m_pps.pps_tc_offset_div2;
    slice->slice_beta_offset_div2 = m_pps.pps_beta_offset_div2;
    slice->slice_loop_filter_across_slices_enabled_flag = m_pps.pps_loop_filter_across_slices_enabled_flag;
    slice->slice_cb_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_cr_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_temporal_mvp_enabled_flag = m_sps.sps_temporal_mvp_enabled_flag;

    if (m_videoParam.BPyramid) {
        Ipp8u num_ref0 = m_videoParam.MaxRefIdxL0;
        Ipp8u num_ref1 = m_videoParam.MaxRefIdxL1;
        if (slice->slice_type == B_SLICE) {
            if (num_ref0 > 1) num_ref0 >>= 1;
            if (num_ref1 > 1) num_ref1 >>= 1;
        }
        slice->num_ref_idx_l0_active = num_ref0;
        slice->num_ref_idx_l1_active = num_ref1;
    }
    else {
        slice->num_ref_idx_l0_active = m_pps.num_ref_idx_l0_default_active;
        slice->num_ref_idx_l1_active = m_pps.num_ref_idx_l1_default_active;
    }

    slice->pgop_idx = slice->slice_type == P_SLICE ? (Ipp8u)m_pCurrentFrame->m_PGOPIndex : 0;
    if (m_videoParam.GeneralizedPB && slice->slice_type == P_SLICE) {
        slice->slice_type = B_SLICE;
        slice->num_ref_idx_l1_active = slice->num_ref_idx_l0_active;
    }


    slice->short_term_ref_pic_set_sps_flag = 1;
    if (m_pCurrentFrame->m_RPSIndex > 0) {
        slice->short_term_ref_pic_set_idx = m_pCurrentFrame->m_RPSIndex;
        if (!m_videoParam.BPyramid)
            slice->num_ref_idx_l0_active = m_videoParam.MaxBRefIdxL0;
    } else if (m_pCurrentFrame->m_PGOPIndex == 0)
        slice->short_term_ref_pic_set_idx = 0;
    else
        slice->short_term_ref_pic_set_idx = m_videoParam.GopRefDist - 1 + m_pCurrentFrame->m_PGOPIndex;

    slice->slice_num = curr_slice;

    slice->five_minus_max_num_merge_cand = 5 - MAX_NUM_MERGE_CANDS;
    
    slice->slice_qp_delta = m_videoParam.m_sliceQpY - m_pps.init_qp;

    if (m_pps.entropy_coding_sync_enabled_flag) {
        slice->row_first = slice->slice_segment_address / m_videoParam.PicWidthInCtbs;
        slice->row_last = slice->slice_address_last_ctb / m_videoParam.PicWidthInCtbs;
        slice->num_entry_point_offsets = slice->row_last - slice->row_first;
    }

    SetAllLambda(slice, m_videoParam.m_sliceQpY, m_pCurrentFrame->m_PicOrderCnt);

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetSlice(H265Slice *slice, Ipp32u curr_slice, H265Frame *frame)
{
    memset(slice, 0, sizeof(H265Slice));

    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    Ipp32u numSlices = m_videoParam.NumSlices;
    Ipp32u slice_len = numCtbs / numSlices;

    slice->slice_segment_address = slice_len * curr_slice;
    slice->slice_address_last_ctb = slice_len * (curr_slice + 1) - 1;
    if (slice->slice_address_last_ctb > numCtbs - 1 || curr_slice == numSlices - 1)
        slice->slice_address_last_ctb = numCtbs - 1;

    if (curr_slice) slice->first_slice_segment_in_pic_flag = 0;
    else slice->first_slice_segment_in_pic_flag = 1;

    slice->slice_pic_parameter_set_id = m_pps.pps_pic_parameter_set_id;

    switch (frame->m_PicCodType) {
    case MFX_FRAMETYPE_P:
        slice->slice_type = P_SLICE; break;
    case MFX_FRAMETYPE_B:
        slice->slice_type = B_SLICE; break;
    case MFX_FRAMETYPE_I:
    default:
        slice->slice_type = I_SLICE; break;
    }

    if (frame->m_bIsIDRPic) {
        slice->IdrPicFlag = 1;
        slice->RapPicFlag = 1;
    }

    slice->slice_pic_order_cnt_lsb =  frame->PicOrderCnt() & ~(0xffffffff << m_sps.log2_max_pic_order_cnt_lsb);
    slice->deblocking_filter_override_flag = 0;
    slice->slice_deblocking_filter_disabled_flag = m_pps.pps_deblocking_filter_disabled_flag;
    slice->slice_tc_offset_div2 = m_pps.pps_tc_offset_div2;
    slice->slice_beta_offset_div2 = m_pps.pps_beta_offset_div2;
    slice->slice_loop_filter_across_slices_enabled_flag = m_pps.pps_loop_filter_across_slices_enabled_flag;
    slice->slice_cb_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_cr_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_temporal_mvp_enabled_flag = m_sps.sps_temporal_mvp_enabled_flag;

    if (m_videoParam.BPyramid) {
        Ipp8u num_ref0 = m_videoParam.MaxRefIdxL0;
        Ipp8u num_ref1 = m_videoParam.MaxRefIdxL1;
        if (slice->slice_type == B_SLICE) {
            if (num_ref0 > 1) num_ref0 >>= 1;
            if (num_ref1 > 1) num_ref1 >>= 1;
        }
        slice->num_ref_idx_l0_active = num_ref0;
        slice->num_ref_idx_l1_active = num_ref1;
    }
    else {
        slice->num_ref_idx_l0_active = m_pps.num_ref_idx_l0_default_active;
        slice->num_ref_idx_l1_active = m_pps.num_ref_idx_l1_default_active;
    }

    slice->pgop_idx = slice->slice_type == P_SLICE ? (Ipp8u)frame->m_PGOPIndex : 0;
    if (m_videoParam.GeneralizedPB && slice->slice_type == P_SLICE) {
        slice->slice_type = B_SLICE;
        slice->num_ref_idx_l1_active = slice->num_ref_idx_l0_active;
    }


    slice->short_term_ref_pic_set_sps_flag = 1;
    if (frame->m_RPSIndex > 0) {
        slice->short_term_ref_pic_set_idx = frame->m_RPSIndex;
        if (!m_videoParam.BPyramid)
            slice->num_ref_idx_l0_active = m_videoParam.MaxBRefIdxL0;
    }
    else if (frame->m_PGOPIndex == 0)
        slice->short_term_ref_pic_set_idx = 0;
    else
        slice->short_term_ref_pic_set_idx = m_videoParam.GopRefDist - 1 + frame->m_PGOPIndex;

    slice->slice_num = curr_slice;

    slice->five_minus_max_num_merge_cand = 5 - MAX_NUM_MERGE_CANDS;
    slice->slice_qp_delta = m_videoParam.m_sliceQpY - m_pps.init_qp;

    if (m_pps.entropy_coding_sync_enabled_flag) {
        slice->row_first = slice->slice_segment_address / m_videoParam.PicWidthInCtbs;
        slice->row_last = slice->slice_address_last_ctb / m_videoParam.PicWidthInCtbs;
        slice->num_entry_point_offsets = slice->row_last - slice->row_first;
    }

    SetAllLambda(slice, m_videoParam.m_sliceQpY, frame->m_PicOrderCnt);    

    return MFX_ERR_NONE;

} //
mfxStatus H265Encoder::SetAllLambda(H265Slice *slice, int qp, int poc, bool isHiCmplxGop, bool isMidCmplxGop)
{
    //-----------------------------------------------------
    // SET LAMBDA
    //-----------------------------------------------------
    slice->rd_opt_flag = 1;
    slice->rd_lambda_slice = 1;
    if (slice->rd_opt_flag) {
        slice->rd_lambda_slice = pow(2.0, (qp - 12) * (1.0/3.0)) * (1.0 / 256.0);
        switch (slice->slice_type) {
        case P_SLICE:
            if (m_videoParam.BPyramid && OPT_LAMBDA_PYRAMID) {
                Ipp8u layer = m_BpyramidRefLayers[poc % m_videoParam.GopRefDist];
                slice->rd_lambda_slice *= tab_rdLambdaBPyramid[layer];
                if (layer > 0)
                    slice->rd_lambda_slice *= MAX(2,MIN(4,(qp - 12)/6.0));
            } else {
                if (m_videoParam.PGopPicSize == 1 || slice->pgop_idx)
                    slice->rd_lambda_slice *= 0.4624;
                else
                    slice->rd_lambda_slice *= 0.578;
                if (slice->pgop_idx)
                    slice->rd_lambda_slice *= MAX(2,MIN(4,(qp - 12)/6));
            }
            break;
        case B_SLICE:
            if (m_videoParam.BPyramid && OPT_LAMBDA_PYRAMID) {
                Ipp8u layer = m_BpyramidRefLayers[poc % m_videoParam.GopRefDist];
                if(m_videoParam.preEncMode > 1)
                {
                    if(isHiCmplxGop) slice->rd_lambda_slice *= tab_rdLambdaBPyramid_HiCmplx[layer];
                    else if(isMidCmplxGop) slice->rd_lambda_slice *= tab_rdLambdaBPyramid_MidCmplx[layer];
                    else slice->rd_lambda_slice *= tab_rdLambdaBPyramid_LoCmplx[layer];
                }
                else
                {
                    slice->rd_lambda_slice *= tab_rdLambdaBPyramid[layer];
                }
                if (layer > 0)
                    slice->rd_lambda_slice *= MAX(2,MIN(4,(qp - 12)/6.0));
            } else {
                slice->rd_lambda_slice *= 0.4624;
                slice->rd_lambda_slice *= MAX(2,MIN(4,(qp - 12)/6));
            }
            break;
        case I_SLICE:
        default:
            slice->rd_lambda_slice *= 0.57;
            if (m_videoParam.GopRefDist > 1)
                slice->rd_lambda_slice *= (1 - MIN(0.5,0.05*(m_videoParam.GopRefDist - 1)));
        }
    }
    slice->rd_lambda_inter_slice = slice->rd_lambda_slice;
    slice->rd_lambda_inter_mv_slice = slice->rd_lambda_inter_slice;

    //kolya
    slice->rd_lambda_sqrt_slice = sqrt(slice->rd_lambda_slice*256);
    //no chroma QP offset (from PPS) is implemented yet
    slice->ChromaDistWeight_slice = pow(2.0, (qp - h265_QPtoChromaQP[qp])/3.0);
    //-----------------------------------------------------
    //-----------------------------------------------------

    return MFX_ERR_NONE;

} // mfxStatus H265Encoder::SetAllLambda(H265Slice *slice, int qp, int poc)

static Ipp8u dpoc_neg[][8][5] = {
    //GopRefDist 4
    {{2, 4, 2},
    {1, 1},
    {1, 2},
    {1, 1}},

    //GopRefDist 8
    {{4, 8, 2, 2, 4},
    {1, 1},
    {2, 2, 2},
    {2, 1, 2},
    {2, 4, 2},
    {2, 1, 4},
    {3, 2, 2, 2},
    {3, 1, 2, 4}},
};

static Ipp8u dpoc_pos[][8][5] = {
    //GopRefDist 4
    {{0},
    {2, 1, 2},
    {1, 2},
    {1, 1}},

    //GopRefDist 8
    {{0,},
    {3, 1, 2, 4},
    {2, 2, 4},
    {2, 1, 4},
    {1, 4},
    {2, 1, 2},
    {1, 2},
    {1, 1}}
};

void H265Encoder::InitShortTermRefPicSet()
{
    m_numShortTermRefPicSets = 0;

    if (m_videoParam.BPyramid && m_videoParam.GopRefDist > 2) {
        Ipp8u r = m_videoParam.GopRefDist == 4 ? 0 : 1;
        for (Ipp32s i = 0; i < 8; i++) {
            m_ShortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
            m_ShortRefPicSet[i].num_negative_pics = dpoc_neg[r][i][0];
            m_ShortRefPicSet[i].num_positive_pics = dpoc_pos[r][i][0];

            for (Ipp32s j = 0; j < m_ShortRefPicSet[i].num_negative_pics; j++) {
                m_ShortRefPicSet[i].delta_poc[j] = dpoc_neg[r][i][1+j];
                m_ShortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
            }
            for (Ipp32s j = 0; j < m_ShortRefPicSet[i].num_positive_pics; j++) {
                m_ShortRefPicSet[i].delta_poc[m_ShortRefPicSet[i].num_negative_pics + j] = dpoc_pos[r][i][1+j];
                m_ShortRefPicSet[i].used_by_curr_pic_flag[m_ShortRefPicSet[i].num_negative_pics + j] = 1;
            }
            m_numShortTermRefPicSets++;
        }
        m_sps.num_short_term_ref_pic_sets = (Ipp8u)m_numShortTermRefPicSets;
        return;
    }

    Ipp32s deltaPoc = m_videoParam.TreatBAsReference ? 1 : m_videoParam.GopRefDist;

    for (Ipp32s i = 0; i < m_videoParam.PGopPicSize; i++) {
        Ipp32s idx = i == 0 ? 0 : m_videoParam.GopRefDist - 1 + i;
        m_ShortRefPicSet[idx].inter_ref_pic_set_prediction_flag = 0;
        m_ShortRefPicSet[idx].num_negative_pics = m_videoParam.MaxRefIdxL0;
        m_ShortRefPicSet[idx].num_positive_pics = 0;
        for (Ipp32s j = 0; j < m_ShortRefPicSet[idx].num_negative_pics; j++) {
            Ipp32s deltaPoc_cur;
            if (j == 0) deltaPoc_cur = 1;
            else if (j == 1) deltaPoc_cur = (m_videoParam.PGopPicSize - 1 + i) % m_videoParam.PGopPicSize;
            else deltaPoc_cur = m_videoParam.PGopPicSize;
            if (deltaPoc_cur == 0) deltaPoc_cur =  m_videoParam.PGopPicSize;
            m_ShortRefPicSet[idx].delta_poc[j] = deltaPoc_cur * m_videoParam.GopRefDist;
            m_ShortRefPicSet[idx].used_by_curr_pic_flag[j] = 1;
        }
        m_numShortTermRefPicSets++;
    }

    for (Ipp32s i = 1; i < m_videoParam.GopRefDist; i++) {
        m_ShortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
        m_ShortRefPicSet[i].num_negative_pics = m_videoParam.MaxBRefIdxL0;
        m_ShortRefPicSet[i].num_positive_pics = 1;

        m_ShortRefPicSet[i].delta_poc[0] = m_videoParam.TreatBAsReference ? 1 : i;
        m_ShortRefPicSet[i].used_by_curr_pic_flag[0] = 1;
        for (Ipp32s j = 1; j < m_ShortRefPicSet[i].num_negative_pics; j++) {
            m_ShortRefPicSet[i].delta_poc[j] = deltaPoc;
            m_ShortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
        }
        for (Ipp32s j = 0; j < m_ShortRefPicSet[i].num_positive_pics; j++) {
            m_ShortRefPicSet[i].delta_poc[m_ShortRefPicSet[i].num_negative_pics + j] = m_videoParam.GopRefDist - i;
            m_ShortRefPicSet[i].used_by_curr_pic_flag[m_ShortRefPicSet[i].num_negative_pics + j] = 1;
        }
        m_numShortTermRefPicSets++;
    }

    m_sps.num_short_term_ref_pic_sets = (Ipp8u)m_numShortTermRefPicSets;
}

#if defined(DUMP_COSTS_CU) || defined (DUMP_COSTS_TU)
extern FILE *fp_cu, *fp_tu;
#endif

mfxStatus H265Encoder::Init(const mfxVideoParam *param, const mfxExtCodingOptionHEVC *optsHevc)
{
#ifdef MFX_ENABLE_WATERMARK
    m_watermark = Watermark::CreateFromResource();
    if (NULL == m_watermark)
        return MFX_ERR_UNKNOWN;
#endif

    mfxStatus sts = InitH265VideoParam(param, optsHevc);
    MFX_CHECK_STS(sts);

#if defined(MFX_ENABLE_H265_PAQ)
    if (m_videoParam.preEncMode)
    {
        mfxVideoParam tmpParam = *param;
        m_preEnc.m_emulatorForSyncPart.Init(tmpParam);
        m_preEnc.m_emulatorForAsyncPart = m_preEnc.m_emulatorForSyncPart;

        m_preEnc.m_stagesToGo = AsyncRoutineEmulator::STG_BIT_CALL_EMULATOR;

        int frameRate = m_videoParam.FrameRateExtN / m_videoParam.FrameRateExtD;
        int refDist =  m_videoParam.GopRefDist;
        int intraPeriod = m_videoParam.GopPicSize;
        int framesToBeEncoded = 610;//for test only
        
        m_preEnc.open(NULL, m_videoParam.Width, m_videoParam.Height, 8, 8, frameRate, refDist, intraPeriod, framesToBeEncoded);

        m_pAQP = NULL;
        m_pAQP = new TAdapQP;
        int g_uiMaxCUWidth = 64;
        int g_uiMaxCUHeight = 64;
        m_pAQP->create(g_uiMaxCUWidth, g_uiMaxCUHeight, m_videoParam.Width, m_videoParam.Height, refDist);
    }
#endif

    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    Ipp32s sizeofH265CU = (m_videoParam.bitDepthLuma > 8 ? sizeof(H265CU<Ipp16u>) : sizeof(H265CU<Ipp8u>));
    
    m_videoParam.m_lcuQps = NULL;
    m_videoParam.m_lcuQps = new Ipp8s[numCtbs];
    
    profile_frequency = m_videoParam.GopRefDist;
    data_temp_size = ((MAX_TOTAL_DEPTH * 2 + 2) << m_videoParam.Log2NumPartInCU);

    // temp buf size - todo reduce
    Ipp32u streamBufSizeMain = m_videoParam.SourceWidth * m_videoParam.SourceHeight * 3 / 2 + DATA_ALIGN;
    Ipp32u streamBufSize = (m_videoParam.SourceWidth * (m_videoParam.threading_by_rows ? m_videoParam.SourceHeight : m_videoParam.MaxCUSize)) * 3 / 2 + DATA_ALIGN;
    Ipp32u memSize = 0;

    memSize += sizeof(H265BsReal)*(m_videoParam.num_thread_structs + 1) + DATA_ALIGN;
    memSize += sizeof(H265BsFake)*m_videoParam.num_thread_structs + DATA_ALIGN;
    memSize += streamBufSize*m_videoParam.num_thread_structs + streamBufSizeMain;
    memSize += sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT * m_videoParam.PicHeightInCtbs;
    memSize += sizeof(H265Slice) * m_videoParam.NumSlices + DATA_ALIGN;
    memSize += sizeof(H265Slice) * m_videoParam.NumSlices + DATA_ALIGN; // for future slice
    memSize += sizeof(H265CUData) * data_temp_size * m_videoParam.num_threads + DATA_ALIGN;
    memSize += numCtbs;
    memSize += sizeof(H265EncoderRowInfo) * m_videoParam.PicHeightInCtbs + DATA_ALIGN;
    //memSize += sizeof(Ipp32u) * profile_frequency + DATA_ALIGN;
    memSize += sizeofH265CU * m_videoParam.num_threads + DATA_ALIGN;
    memSize += (1 << 16); // for m_logMvCostTable
    memSize += numCtbs * sizeof(costStat) + DATA_ALIGN;

    memBuf = (Ipp8u *)H265_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(memBuf);

    Ipp8u *ptr = memBuf;

    bs = UMC::align_pointer<H265BsReal*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265BsReal)*(m_videoParam.num_thread_structs + 1) + DATA_ALIGN;
    bsf = UMC::align_pointer<H265BsFake*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265BsFake)*(m_videoParam.num_thread_structs) + DATA_ALIGN;

    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs+1; i++) {
        bs[i].m_base.m_pbsBase = ptr;
        bs[i].m_base.m_maxBsSize = streamBufSize;
        bs[i].Reset();
        ptr += streamBufSize;
    }
    bs[m_videoParam.num_thread_structs].m_base.m_maxBsSize = streamBufSizeMain;
    ptr += streamBufSizeMain - streamBufSize;

    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs; i++)
        bsf[i].Reset();

    m_context_array_wpp = ptr;
    ptr += sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT * m_videoParam.PicHeightInCtbs;

    m_slices = UMC::align_pointer<H265Slice*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265Slice) * m_videoParam.NumSlices + DATA_ALIGN;

    m_slicesNext = UMC::align_pointer<H265Slice*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265Slice) * m_videoParam.NumSlices + DATA_ALIGN;

    data_temp = UMC::align_pointer<H265CUData*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265CUData) * data_temp_size * m_videoParam.num_threads + DATA_ALIGN;

    m_slice_ids = ptr;
    ptr += numCtbs;

    m_row_info = UMC::align_pointer<H265EncoderRowInfo*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265EncoderRowInfo) * m_videoParam.PicHeightInCtbs + DATA_ALIGN;

    cu = UMC::align_pointer<void*>(ptr, DATA_ALIGN);
    ptr += sizeofH265CU * m_videoParam.num_threads + DATA_ALIGN;

    m_logMvCostTable = ptr;
    ptr += (1 << 16);

    m_costStat = (costStat *)ptr;
    ptr += numCtbs * sizeof(costStat);

    // init lookup table for 2*log2(x)+2
    m_logMvCostTable[(1 << 15)] = 1;
    Ipp32f log2reciproc = 2 / log(2.0f);
    for (Ipp32s i = 1; i < (1 << 15); i++)
        m_logMvCostTable[(1 << 15) + i] = m_logMvCostTable[(1 << 15) - i] = (Ipp8u)(log(i + 1.0f) * log2reciproc + 2);

    m_iProfileIndex = 0;
    //m_bMakeNextFrameKey = true; // Ensure that we always start with a key frame.
    m_bMakeNextFrameIDR = true;
    m_uIntraFrameInterval = 0;
    m_uIDRFrameInterval = 0;
    m_l1_cnt_to_start_B = 0;
    m_PicOrderCnt = 0;
    m_PicOrderCnt_Accu = 0;
    m_PGOPIndex = 0;
    m_frameCountEncoded = 0;
    m_frameCountSend = 0;

    m_Bpyramid_currentNumFrame = 0;
    m_Bpyramid_nextNumFrame = 0;
    m_Bpyramid_maxNumFrame = 0;

    if (m_videoParam.BPyramid) {
        Ipp8u n = 0;
        Ipp32s GopRefDist = m_videoParam.GopRefDist;

        while (((GopRefDist & 1) == 0) && GopRefDist > 1) {
            GopRefDist >>= 1;
            n ++;
        }

        m_Bpyramid_maxNumFrame = 1 << n;
        m_BpyramidTab[0] = 0;
        m_BpyramidTab[(Ipp32u)(1 << n)] = 0;
        m_BpyramidTabRight[0] = 0;
        m_BpyramidRefLayers[0] = 0;

        for (Ipp32s i = n - 1, j = 0; i >= 0; i--, j++) {
            for (Ipp32s k = 0; k < (1 << j); k++) {
                if ((k & 1) == 0)
                    m_BpyramidTab[(1<<i)+(k*(1<<(i+1)))] = m_BpyramidTab[(k+1)*(1<<(i+1))] + 1;
                else
                    m_BpyramidTab[(1<<i)+(k*(1<<(i+1)))] = m_BpyramidTab[k*(1<<(i+1))] + (1 << (i+1));

                m_BpyramidTabRight[m_BpyramidTab[(1<<i)+(k*(1<<(i+1)))]] = m_BpyramidTab[(1<<(i+1))+(k*(1<<(i+1)))];
                m_BpyramidRefLayers[(1<<i)+(k*(1<<(i+1)))] = (Ipp8u)(j + 1);
            }
        }
    }
    m_BpyramidRefLayers[m_videoParam.GopRefDist] = 0;

    if (param->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        m_brc = new H265BRC();
        mfxStatus sts = m_brc->Init(param, m_videoParam.bitDepthLuma);
        if (MFX_ERR_NONE != sts)
            return sts;
        m_videoParam.m_sliceQpY = (Ipp8s)m_brc->GetQP(MFX_FRAMETYPE_I);
    }
    else {
        m_videoParam.m_sliceQpY = m_videoParam.QPI;
    }

    memset(m_videoParam.cu_split_threshold_cu, 0, 52 * 2 * MAX_TOTAL_DEPTH * sizeof(Ipp64f));
    memset(m_videoParam.cu_split_threshold_tu, 0, 52 * 2 * MAX_TOTAL_DEPTH * sizeof(Ipp64f));

    Ipp32s qpMax = 42;
    for (Ipp32s QP = 10; QP <= qpMax; QP++) {
        for (Ipp32s isNotI = 0; isNotI <= 1; isNotI++) {
            for (Ipp32s i = 0; i < m_videoParam.MaxCUDepth; i++) {
                m_videoParam.cu_split_threshold_cu[QP][isNotI][i] = h265_calc_split_threshold(param->mfx.TargetUsage, 0, isNotI, m_videoParam.Log2MaxCUSize - i,
                    isNotI ? m_videoParam.SplitThresholdStrengthCUInter : m_videoParam.SplitThresholdStrengthCUIntra, QP);
                m_videoParam.cu_split_threshold_tu[QP][isNotI][i] = h265_calc_split_threshold(param->mfx.TargetUsage, 1, isNotI, m_videoParam.Log2MaxCUSize - i,
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

    m_videoParam.csps = &m_sps;
    m_videoParam.cpps = &m_pps;

    m_pReconstructFrame = new H265Frame();
    if (m_pReconstructFrame->Create(&m_videoParam) != MFX_ERR_NONE )
        return MFX_ERR_MEMORY_ALLOC;

    m_videoParam.m_slice_ids = m_slice_ids;
    m_videoParam.m_costStat = m_costStat;

    ippsZero_8u((Ipp8u *)cu, sizeofH265CU * m_videoParam.num_threads);
    ippsZero_8u((Ipp8u*)data_temp, sizeof(H265CUData) * data_temp_size * m_videoParam.num_threads);

    MFX_HEVC_PP::InitDispatcher();

    if (m_videoParam.SAOFlag) {
        m_saoParam.resize(m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs);

        for(Ipp32u idx = 0; idx < m_videoParam.num_threads; idx++)
        {
            if (m_videoParam.bitDepthLuma == 8) {
                ((H265CU<Ipp8u>*)cu)[idx].m_saoEncodeFilter.Init(m_videoParam.Width, m_videoParam.Height,
                    1 << m_videoParam.Log2MaxCUSize, m_videoParam.MaxCUDepth, m_videoParam.bitDepthLuma, 
                    m_videoParam.saoOpt);
            } else {
                ((H265CU<Ipp16u>*)cu)[idx].m_saoEncodeFilter.Init(m_videoParam.Width, m_videoParam.Height,
                    1 << m_videoParam.Log2MaxCUSize, m_videoParam.MaxCUDepth, m_videoParam.bitDepthLuma,
                    m_videoParam.saoOpt);
            }
        }
    }

#if defined (MFX_ENABLE_CM)
    if (m_videoParam.enableCmFlag) {
        Ipp8u nRef = m_videoParam.csps->sps_max_dec_pic_buffering[0] + 1;
        AllocateCmResources(param->mfx.FrameInfo.Width, param->mfx.FrameInfo.Height, nRef);
    }
#endif // MFX_ENABLE_CM

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

    return sts;
}

void H265Encoder::Close() {

#if defined (MFX_ENABLE_CM)
    if (m_videoParam.enableCmFlag) {
#if defined(_WIN32) || defined(_WIN64)
        PrintTimes();
#endif // #if defined(_WIN32) || defined(_WIN64)
        FreeCmResources();
    }
#endif // MFX_ENABLE_CM

#if defined(MFX_ENABLE_H265_PAQ)
    if (m_videoParam.preEncMode) {
        m_preEnc.close();
    }
    if(m_pAQP)
    {
        m_pAQP->destroy();
        delete m_pAQP;
    }
#endif
    if (m_pReconstructFrame) {
        m_pReconstructFrame->Destroy();
        delete m_pReconstructFrame;
    }
    if (m_brc)
        delete m_brc;

    if (memBuf) {
        H265_Free(memBuf);
        memBuf = NULL;
    }

    if (NULL != m_videoParam.m_lcuQps) {
        delete [] m_videoParam.m_lcuQps;
        m_videoParam.m_lcuQps = NULL;
    }

#ifdef DUMP_COSTS_CU
    if (fp_cu) fclose(fp_cu);
#endif
#ifdef DUMP_COSTS_TU
    if (fp_tu) fclose(fp_tu);
#endif

#ifdef MFX_ENABLE_WATERMARK
    if (m_watermark)
        m_watermark->Release();
#endif
}

Ipp32u H265Encoder::DetermineFrameType()
{
    // GOP is considered here as I frame and all following till next I
    // Display (==input) order
    Ipp32u ePictureType; //= eFrameType[m_iProfileIndex];

    m_bMakeNextFrameIDR = false;

    if (m_frameCountSend == 0) {
        m_iProfileIndex = 0;
        m_uIntraFrameInterval = 0;
        m_uIDRFrameInterval = 0;
        ePictureType = MFX_FRAMETYPE_I;
        m_bMakeNextFrameIDR = true;
        return ePictureType;
    }

    m_iProfileIndex++;
    if (m_iProfileIndex == profile_frequency) {
        m_iProfileIndex = 0;
        ePictureType = MFX_FRAMETYPE_P;
    }
    else {
        ePictureType = MFX_FRAMETYPE_B;
        if (m_uIntraFrameInterval+2 == m_videoParam.GopPicSize &&
            (m_videoParam.GopClosedFlag || m_uIDRFrameInterval == m_videoParam.IdrInterval) )
            if (!m_videoParam.BPyramid && !m_videoParam.GopStrictFlag)
                ePictureType = MFX_FRAMETYPE_P;
            // else ?? // B-group will start next GOP using only L1 ref
    }

    m_uIntraFrameInterval++;
    if (m_uIntraFrameInterval == m_videoParam.GopPicSize) {
        ePictureType = MFX_FRAMETYPE_I;
        m_uIntraFrameInterval = 0;
        m_iProfileIndex = 0;

        m_uIDRFrameInterval++;
        if (m_uIDRFrameInterval == m_videoParam.IdrInterval + 1) {
            m_bMakeNextFrameIDR = true;
            m_uIDRFrameInterval = 0;
        }
    }

    return ePictureType;
}

//
// move all frames in WaitingForRef to ReadyToEncode
//
mfxStatus H265Encoder::MoveFromCPBToDPB()
{
    m_cpb.RemoveFrame(m_pCurrentFrame);
    m_dpb.insertAtCurrent(m_pCurrentFrame);
    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::CleanDPB()
{
    H265Frame *pFrm = m_dpb.findNextDisposable();
    mfxStatus ps = MFX_ERR_NONE;
    while (pFrm != NULL) {
        m_dpb.RemoveFrame(pFrm);
        m_cpb.insertAtCurrent(pFrm);
        pFrm=m_dpb.findNextDisposable();
    }
    return ps;
}

void H265Encoder::CreateRefPicSet(H265Slice *slice, H265Frame *frame)
{
    Ipp32s currPicOrderCnt = frame->PicOrderCnt();
    H265Frame **refFrames = frame->m_refPicList[0].m_refFrames;
    H265ShortTermRefPicSet* refPicSet;
    Ipp32s excludedPOC = -1;
    Ipp32u numShortTermL0, numShortTermL1, numLongTermL0, numLongTermL1;

    m_dpb.countActiveRefs(numShortTermL0, numLongTermL0, numShortTermL1, numLongTermL1, currPicOrderCnt);

    if ((numShortTermL0 + numShortTermL1 + numLongTermL0 + numLongTermL1) >= (Ipp32u)m_sps.num_ref_frames) {
        if ((numShortTermL0 + numShortTermL1) > 0) {
            H265Frame *frm = m_dpb.findMostDistantShortTermRefs(currPicOrderCnt);
            excludedPOC = frm->PicOrderCnt();

            if (excludedPOC < currPicOrderCnt)
                numShortTermL0--;
            else
                numShortTermL1--;
        }
        else {
            H265Frame *frm = m_dpb.findMostDistantLongTermRefs(currPicOrderCnt);
            excludedPOC = frm->PicOrderCnt();

            if (excludedPOC < currPicOrderCnt)
                numLongTermL0--;
            else
                numLongTermL1--;
        }
    }

    slice->short_term_ref_pic_set_sps_flag = 0;
    refPicSet = m_ShortRefPicSet + m_sps.num_short_term_ref_pic_sets;
    refPicSet->inter_ref_pic_set_prediction_flag = 0;

    /* Long term TODO */

    /* Short Term L0 */
    if (numShortTermL0 > 0) {
        H265Frame *pHead = m_dpb.head();
        H265Frame *pFrm;
        Ipp32s NumFramesInList = 0;
        Ipp32s prev, numRefs;

        for (pFrm = pHead; pFrm; pFrm = pFrm->future()) {
            Ipp32s poc = pFrm->PicOrderCnt();

            if (pFrm->isShortTermRef() && (poc < currPicOrderCnt) && (poc != excludedPOC)) {
                // find insertion point
                Ipp32s j = 0;
                while ((j < NumFramesInList) && (refFrames[j]->PicOrderCnt() > poc))
                    j++;

                // make room if needed
                if (j < NumFramesInList && refFrames[j])
                    for (Ipp32s k = NumFramesInList; k > j; k--)
                        refFrames[k] = refFrames[k-1];

                // add the short-term reference
                refFrames[j] = pFrm;
                NumFramesInList++;
            }
        }

        refPicSet->num_negative_pics = (Ipp8u)NumFramesInList;

        prev = 0;
        for (Ipp32s i = 0; i < refPicSet->num_negative_pics; i++) {
            Ipp32s DeltaPoc = refFrames[i]->PicOrderCnt() - currPicOrderCnt;
            refPicSet->delta_poc[i] = prev - DeltaPoc;
            prev = DeltaPoc;
        }

        if (slice->slice_type == I_SLICE)
            numRefs = 0;
        else
            numRefs = (Ipp32s)slice->num_ref_idx_l0_active;

        Ipp32s used_count = 0;
        for (Ipp32s i = 0; i < refPicSet->num_negative_pics; i++) {
            if (used_count < numRefs && refFrames[i]->m_PGOPIndex == 0) {
                refPicSet->used_by_curr_pic_flag[i] = 1;
                used_count++;
            }
            else {
                refPicSet->used_by_curr_pic_flag[i] = 0;
            }
        }
        for (Ipp32s i = 0; i < refPicSet->num_negative_pics; i++) {
            if (used_count < numRefs && refPicSet->used_by_curr_pic_flag[i] == 0) {
                refPicSet->used_by_curr_pic_flag[i] = 1;
                used_count++;
            }
        }
    }
    else {
        refPicSet->num_negative_pics = 0;
    }

    /* Short Term L1 */
    if (numShortTermL1 > 0) {
        H265Frame *pHead = m_dpb.head();
        H265Frame *pFrm;
        Ipp32s NumFramesInList = 0;
        Ipp32s prev, numRefs;

        for (pFrm = pHead; pFrm; pFrm = pFrm->future()) {
            Ipp32s poc = pFrm->PicOrderCnt();

            if (pFrm->isShortTermRef()&& (poc > currPicOrderCnt) && (poc != excludedPOC)) {
                // find insertion point
                Ipp32s j = 0;
                while ((j < NumFramesInList) && (refFrames[j]->PicOrderCnt() < poc))
                    j++;

                // make room if needed
                if (j < NumFramesInList && refFrames[j])
                    for (Ipp32s k = NumFramesInList; k > j; k--)
                        refFrames[k] = refFrames[k-1];

                // add the short-term reference
                refFrames[j] = pFrm;
                NumFramesInList++;
            }
        }

        refPicSet->num_positive_pics = (Ipp8u)NumFramesInList;

        prev = 0;
        for (Ipp32s i = 0; i < refPicSet->num_positive_pics; i++) {
            Ipp32s DeltaPoc = refFrames[i]->PicOrderCnt() - currPicOrderCnt;
            refPicSet->delta_poc[refPicSet->num_negative_pics + i] = DeltaPoc - prev;
            prev = DeltaPoc;
        }

        if (slice->slice_type == B_SLICE)
            numRefs = m_videoParam.MaxRefIdxL1 + 1;
        else
            numRefs = 0;

        if (numRefs > refPicSet->num_positive_pics)
            numRefs = refPicSet->num_positive_pics;

        for (Ipp32s i = 0; i < numRefs; i++)
            refPicSet->used_by_curr_pic_flag[refPicSet->num_negative_pics + i] = 1;

        for (Ipp32s i = numRefs; i < refPicSet->num_positive_pics; i++)
            refPicSet->used_by_curr_pic_flag[refPicSet->num_negative_pics + i] = 0;
    }
    else {
        refPicSet->num_positive_pics = 0;
    }
}

mfxStatus H265Encoder::CheckRefPicSet(H265Slice *curr_slice, H265Frame *frame)
{
    Ipp32s currPicOrderCnt = frame->PicOrderCnt();
    Ipp32s maxPicOrderCntLsb = (1 << m_sps.log2_max_pic_order_cnt_lsb);

    if (frame->m_bIsIDRPic)
        return MFX_ERR_NONE;

    if (!curr_slice->short_term_ref_pic_set_sps_flag)
        return MFX_ERR_UNKNOWN;

    H265ShortTermRefPicSet *refPicSet = m_ShortRefPicSet + curr_slice->short_term_ref_pic_set_idx;

    for (Ipp32s i = 0; i < (Ipp32s)curr_slice->num_long_term_pics; i++) {
        Ipp32s poc = (currPicOrderCnt - curr_slice->poc_lsb_lt[i] + maxPicOrderCntLsb) & (maxPicOrderCntLsb - 1);
        if (curr_slice->delta_poc_msb_present_flag[i])
            poc -= curr_slice->delta_poc_msb_cycle_lt[i] * maxPicOrderCntLsb;

        H265Frame *frm = m_dpb.findFrameByPOC(poc);
        if (frm == NULL)
            return MFX_ERR_UNKNOWN;
    }

    Ipp32s prev = 0;
    for (Ipp32s i = 0; i < refPicSet->num_negative_pics; i++) {
        Ipp32s deltaPoc = prev - refPicSet->delta_poc[i];
        prev = deltaPoc;

        H265Frame *frm = m_dpb.findFrameByPOC(currPicOrderCnt + deltaPoc);
        if (frm == NULL || !frm->isShortTermRef())
            return MFX_ERR_UNKNOWN;
    }

    prev = 0;
    for (Ipp32s i = refPicSet->num_negative_pics; i < refPicSet->num_positive_pics + refPicSet->num_negative_pics; i++) {
        Ipp32s deltaPoc = prev + refPicSet->delta_poc[i];
        prev = deltaPoc;

        H265Frame *frm = m_dpb.findFrameByPOC(currPicOrderCnt + deltaPoc);
        if (frm == NULL || !frm->isShortTermRef())
            return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::UpdateRefPicList(H265Slice *curr_slice, H265Frame *frame)
{
    RefPicList *refPicList = frame->m_refPicList;
    H265ShortTermRefPicSet* refPicSet;
    H265Frame *refPicSetStCurrBefore[MAX_NUM_REF_FRAMES];
    H265Frame *refPicSetStCurrAfter[MAX_NUM_REF_FRAMES];
    H265Frame *refPicSetLtCurr[MAX_NUM_REF_FRAMES];
    H265Frame *refPicListTemp[MAX_NUM_REF_FRAMES];
    H265Frame *frm;
    H265Frame **pRefPicList0 = refPicList[0].m_refFrames;
    H265Frame **pRefPicList1 = refPicList[1].m_refFrames;
    Ipp8s  *pTb0 = refPicList[0].m_deltaPoc;
    Ipp8s  *pTb1 = refPicList[1].m_deltaPoc;
    Ipp32s numPocStCurrBefore, numPocStCurrAfter, numPocLtCurr;
    Ipp32s currPicOrderCnt = frame->PicOrderCnt();
    Ipp32s maxPicOrderCntLsb = (1 << m_sps.log2_max_pic_order_cnt_lsb);
    EnumSliceType slice_type = curr_slice->slice_type;
    Ipp32s prev;

    VM_ASSERT(frame);

    if (!curr_slice->short_term_ref_pic_set_sps_flag)
        refPicSet = m_ShortRefPicSet + m_sps.num_short_term_ref_pic_sets;
    else
        refPicSet = m_ShortRefPicSet + curr_slice->short_term_ref_pic_set_idx;

    m_dpb.unMarkAll();

    numPocLtCurr = 0;

    for (Ipp32s i = 0; i < (Ipp32s)curr_slice->num_long_term_pics; i++) {
        Ipp32s poc = (currPicOrderCnt - curr_slice->poc_lsb_lt[i] + maxPicOrderCntLsb) & (maxPicOrderCntLsb - 1);
        if (curr_slice->delta_poc_msb_present_flag[i])
            poc -= curr_slice->delta_poc_msb_cycle_lt[i] * maxPicOrderCntLsb;

        frm = m_dpb.findFrameByPOC(poc);
        if (frm == NULL) {
            if (!frame->m_bIsIDRPic) {
                VM_ASSERT(0);
            }
        }
        else {
            frm->m_isMarked = true;
            frm->unSetisShortTermRef();
            frm->SetisLongTermRef();
        }

        if (curr_slice->used_by_curr_pic_lt_flag[i])
        {
            refPicSetLtCurr[numPocLtCurr] = frm;
            numPocLtCurr++;
        }
    }

    numPocStCurrBefore = 0;
    numPocStCurrAfter = 0;

    prev = 0;
    for (Ipp32s i = 0; i < refPicSet->num_negative_pics; i++) {
        Ipp32s deltaPoc = prev - refPicSet->delta_poc[i];
        prev = deltaPoc;

        frm = m_dpb.findFrameByPOC(currPicOrderCnt + deltaPoc);
        if (frm == NULL || !frm->isShortTermRef()) {
            if (!frame->m_bIsIDRPic) {
                VM_ASSERT(0);
            }
        }
        else {
            frm->m_isMarked = true;
            frm->unSetisLongTermRef();
            frm->SetisShortTermRef();
        }

        if (refPicSet->used_by_curr_pic_flag[i]) {
            refPicSetStCurrBefore[numPocStCurrBefore] = frm;
            numPocStCurrBefore++;
        }
    }

    prev = 0;
    for (Ipp32s i = refPicSet->num_negative_pics; i < refPicSet->num_positive_pics + refPicSet->num_negative_pics; i++) {
        Ipp32s deltaPoc = prev + refPicSet->delta_poc[i];
        prev = deltaPoc;

        frm = m_dpb.findFrameByPOC(currPicOrderCnt + deltaPoc);
        if (frm == NULL || !frm->isShortTermRef()) {
            if (!frame->m_bIsIDRPic) {
                VM_ASSERT(0);
            }
        }
        else {
            frm->m_isMarked = true;
            frm->unSetisLongTermRef();
            frm->SetisShortTermRef();
        }

        if (refPicSet->used_by_curr_pic_flag[i]) {
            refPicSetStCurrAfter[numPocStCurrAfter] = frm;
            numPocStCurrAfter++;
        }
    }

    m_dpb.removeAllUnmarkedRef();

    curr_slice->m_NumRefsInL0List = 0;
    curr_slice->m_NumRefsInL1List = 0;

    if (curr_slice->slice_type == I_SLICE) {
        curr_slice->num_ref_idx_l0_active = 0;
        curr_slice->num_ref_idx_l1_active = 0;
    }
    else {
        /* create a LIST0 */
        curr_slice->m_NumRefsInL0List = numPocStCurrBefore + numPocStCurrAfter + numPocLtCurr;

        Ipp32s j = 0;
        for (Ipp32s i = 0; i < numPocStCurrBefore; i++, j++)
            refPicListTemp[j] = refPicSetStCurrBefore[i];

        for (Ipp32s i = 0; i < numPocStCurrAfter; i++, j++)
            refPicListTemp[j] = refPicSetStCurrAfter[i];

        for (Ipp32s i = 0; i < numPocLtCurr; i++, j++)
            refPicListTemp[j] = refPicSetLtCurr[i];

        if (curr_slice->m_NumRefsInL0List > (Ipp32s)curr_slice->num_ref_idx_l0_active)
            curr_slice->m_NumRefsInL0List = (Ipp32s)curr_slice->num_ref_idx_l0_active;

        if (curr_slice->m_ref_pic_list_modification_flag_l0)
            for (Ipp32s i = 0; i < curr_slice->m_NumRefsInL0List; i++)
                pRefPicList0[i] = refPicListTemp[curr_slice->list_entry_l0[i]];
        else
            for (Ipp32s i = 0; i < curr_slice->m_NumRefsInL0List; i++)
                pRefPicList0[i] = refPicListTemp[i];

        if (curr_slice->slice_type == B_SLICE) {
            /* create a LIST1 */
            curr_slice->m_NumRefsInL1List = numPocStCurrBefore + numPocStCurrAfter + numPocLtCurr;

            j = 0;
            for (Ipp32s i = 0; i < numPocStCurrAfter; i++, j++)
                refPicListTemp[j] = refPicSetStCurrAfter[i];

            for (Ipp32s i = 0; i < numPocStCurrBefore; i++, j++)
                refPicListTemp[j] = refPicSetStCurrBefore[i];

            for (Ipp32s i = 0; i < numPocLtCurr; i++, j++)
                refPicListTemp[j] = refPicSetLtCurr[i];

            if (curr_slice->m_NumRefsInL1List > (Ipp32s)curr_slice->num_ref_idx_l1_active)
                curr_slice->m_NumRefsInL1List = (Ipp32s)curr_slice->num_ref_idx_l1_active;

            if (curr_slice->m_ref_pic_list_modification_flag_l1)
                for (Ipp32s i = 0; i < curr_slice->m_NumRefsInL1List; i++)
                    pRefPicList1[i] = refPicListTemp[curr_slice->list_entry_l1[i]];
            else
                for (Ipp32s i = 0; i < curr_slice->m_NumRefsInL1List; i++)
                    pRefPicList1[i] = refPicListTemp[i];

            /* create a LIST_C */
            /* TO DO */
        }

        for (Ipp32s i = 0; i < curr_slice->m_NumRefsInL0List; i++) {
            Ipp32s dPOC = currPicOrderCnt - pRefPicList0[i]->PicOrderCnt();

            if (dPOC < -128)     dPOC = -128;
            else if (dPOC > 127) dPOC =  127;

            pTb0[i] = (Ipp8s)dPOC;
        }

        for (Ipp32s i = 0; i < curr_slice->m_NumRefsInL1List; i++) {
            Ipp32s dPOC = currPicOrderCnt - pRefPicList1[i]->PicOrderCnt();

            if (dPOC < -128)     dPOC = -128;
            else if (dPOC > 127) dPOC =  127;

            pTb1[i] = (Ipp8s)dPOC;
        }

        curr_slice->num_ref_idx_l0_active = MAX(curr_slice->m_NumRefsInL0List, 1);
        curr_slice->num_ref_idx_l1_active = MAX(curr_slice->m_NumRefsInL1List, 1);

        if (curr_slice->slice_type == P_SLICE) {
            curr_slice->num_ref_idx_l1_active = 0;
            curr_slice->num_ref_idx_active_override_flag =
                (curr_slice->num_ref_idx_l0_active != m_pps.num_ref_idx_l0_default_active);
        }
        else {
            curr_slice->num_ref_idx_active_override_flag = (
                (curr_slice->num_ref_idx_l0_active != m_pps.num_ref_idx_l0_default_active) ||
                (curr_slice->num_ref_idx_l1_active != m_pps.num_ref_idx_l1_default_active));
        }

        /* RD tmp current version of HM decoder doesn't support m_num_ref_idx_active_override_flag == 0 yet*/
        curr_slice->num_ref_idx_active_override_flag = 1;

        // If default
        if ((P_SLICE == slice_type && m_pps.weighted_pred_flag == 0) ||
            (B_SLICE == slice_type && m_pps.weighted_bipred_flag == 0))
        {
            curr_slice->luma_log2_weight_denom = 0;
            curr_slice->chroma_log2_weight_denom = 0;
            for (Ipp8u i = 0; i < curr_slice->num_ref_idx_l0_active; i++) // refIdxL0
                for (Ipp8u j = 0; j < curr_slice->num_ref_idx_l1_active; j++) // refIdxL1
                    for (Ipp8u k = 0; k < 6; k++) { // L0/L1 and planes
                        curr_slice->weights[k][i][j] = 1;
                        curr_slice->offsets[k][i][j] = 0;
                    }
        }

        //update temporal refpiclists
        for (Ipp32s i = 0; i < (Ipp32s)curr_slice->num_ref_idx_l0_active; i++) {
            frame->m_refPicList[0].m_refFrames[i] = pRefPicList0[i];
            frame->m_refPicList[0].m_deltaPoc[i] = pTb0[i];
            frame->m_refPicList[0].m_isLongTermRef[i] = pRefPicList0[i]->isLongTermRef();
        }

        for (Ipp32s i = 0; i < (Ipp32s)curr_slice->num_ref_idx_l1_active; i++) {
            frame->m_refPicList[1].m_refFrames[i] = pRefPicList1[i];
            frame->m_refPicList[1].m_deltaPoc[i] = pTb1[i];
            frame->m_refPicList[1].m_isLongTermRef[i] = pRefPicList1[i]->isLongTermRef();
        }
    }

    return MFX_ERR_NONE;
}

template <typename PixType>
mfxStatus H265Encoder::DeblockThread(Ipp32s ithread)
{
    H265CU<PixType> *cu = (H265CU<PixType> *)this->cu;
    Ipp32u ctb_row;
    H265VideoParam *pars = &m_videoParam;

    while (1) {
        if (pars->num_threads > 1)
            ctb_row = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&m_incRow)) - 1;
        else
            ctb_row = m_incRow++;

        if (ctb_row >= pars->PicHeightInCtbs)
            break;

        Ipp32u ctb_addr = ctb_row * pars->PicWidthInCtbs;

        for (Ipp32u ctb_col = 0; ctb_col < pars->PicWidthInCtbs; ctb_col ++, ctb_addr++) {
            Ipp8u curr_slice = m_slice_ids[ctb_addr];

            cu[ithread].InitCu(&m_videoParam, m_pReconstructFrame->cu_data + (ctb_addr << pars->Log2NumPartInCU),
                data_temp + ithread * data_temp_size, ctb_addr, (PixType *)m_pReconstructFrame->y,
                (PixType *)m_pReconstructFrame->uv, m_pReconstructFrame->pitch_luma_pix, m_pCurrentFrame,
                &bsf[ithread], m_slices + curr_slice, 0, m_logMvCostTable);
            
            if (pars->UseDQP)
                cu[ithread].UpdateCuQp();
            
            cu[ithread].Deblock();
        }
    }
    return MFX_ERR_NONE;
}

template mfxStatus H265Encoder::DeblockThread<Ipp8u>(Ipp32s ithread);
template mfxStatus H265Encoder::DeblockThread<Ipp16u>(Ipp32s ithread);

template <typename PixType>
mfxStatus H265Encoder::ApplySAOThread(Ipp32s ithread)
{
    IppiSize roiSize;
    roiSize.width = m_videoParam.Width;
    roiSize.height = m_videoParam.Height;

    Ipp32s numCTU = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;

    PixType* pRec[2] = {(PixType*)m_pReconstructFrame->y, (PixType*)m_pReconstructFrame->uv};
    Ipp32s pitch[3] = {m_pReconstructFrame->pitch_luma_pix, m_pReconstructFrame->pitch_chroma_pix >> 1, m_pReconstructFrame->pitch_chroma_pix >> 1}; // FIXME10BIT
    Ipp32s shift[3] = {0, 1, 1};

    SaoDecodeFilter<PixType> saoFilter_New[NUM_USED_SAO_COMPONENTS];

    for (Ipp32s compId = 0; compId < NUM_USED_SAO_COMPONENTS; compId++) {
        saoFilter_New[compId].Init(m_videoParam.Width, m_videoParam.Height, m_videoParam.MaxCUSize, 0,
            compId < 1 ? m_videoParam.bitDepthLuma : m_videoParam.bitDepthChroma);

        // save boundaries
        Ipp32s width = roiSize.width >> shift[compId];
            small_memcpy(saoFilter_New[compId].m_TmpU[0], pRec[compId], sizeof(PixType) * width);
    }

    // ----------------------------------------------------------------------------------------

    for (Ipp32s ctu = 0; ctu < numCTU; ctu++) {
        for (Ipp32s compId = 0; compId < NUM_USED_SAO_COMPONENTS; compId++) {
            // update #1
            Ipp32s ctb_pelx = ( ctu % m_videoParam.PicWidthInCtbs ) * m_videoParam.MaxCUSize;
            Ipp32s ctb_pely = ( ctu / m_videoParam.PicWidthInCtbs ) * m_videoParam.MaxCUSize;

            Ipp32s offset = (ctb_pelx >> shift[compId]) + (ctb_pely >> shift[compId]) * pitch[compId];

            if ((ctu % m_videoParam.PicWidthInCtbs) == 0 && (ctu / m_videoParam.PicWidthInCtbs) != (m_videoParam.PicHeightInCtbs - 1)) {
                PixType* pRecTmp = pRec[compId] + offset + ( (m_videoParam.MaxCUSize >> shift[compId]) - 1)*pitch[compId];
                small_memcpy(saoFilter_New[compId].m_TmpU[1], pRecTmp, sizeof(PixType) * (roiSize.width >> shift[compId]) );
            }

            if ((ctu % m_videoParam.PicWidthInCtbs) == 0)
                for (Ipp32u i = 0; i < (m_videoParam.MaxCUSize >> shift[compId])+1; i++)
                    saoFilter_New[compId].m_TmpL[0][i] = pRec[compId][offset + i*pitch[compId]];

            if ((ctu % m_videoParam.PicWidthInCtbs) != (m_videoParam.PicWidthInCtbs - 1))
                for (Ipp32u i = 0; i < (m_videoParam.MaxCUSize >> shift[compId])+1; i++)
                    saoFilter_New[compId].m_TmpL[1][i] = pRec[compId][offset + i*pitch[compId] + (m_videoParam.MaxCUSize >> shift[compId])-1];

            if (m_saoParam[ctu][compId].mode_idx != SAO_MODE_OFF) {
                if (m_saoParam[ctu][compId].mode_idx == SAO_MODE_MERGE) {
                    // need to restore SAO parameters
                    //reconstructBlkSAOParam(m_saoParam[ctu], mergeList);
                }
                saoFilter_New[compId].SetOffsetsLuma(m_saoParam[ctu][compId], m_saoParam[ctu][compId].type_idx);

                h265_ProcessSaoCuOrg_Luma(
                    pRec[compId] + offset,
                    pitch[compId],
                    m_saoParam[ctu][compId].type_idx,
                    saoFilter_New[compId].m_TmpL[0],
                    &(saoFilter_New[compId].m_TmpU[0][ctb_pelx >> shift[compId]]),
                    m_videoParam.MaxCUSize >> shift[compId],
                    m_videoParam.MaxCUSize >> shift[compId],
                    roiSize.width >> shift[compId],
                    roiSize.height >> shift[compId],
                    saoFilter_New[compId].m_OffsetEo,
                    saoFilter_New[compId].m_OffsetBo,
                    saoFilter_New[compId].m_ClipTable,
                    ctb_pelx >> shift[compId],
                    ctb_pely >> shift[compId]/*,borders*/);
            }

            std::swap(saoFilter_New[compId].m_TmpL[0], saoFilter_New[compId].m_TmpL[1]);

            if (((ctu + 1) % m_videoParam.PicWidthInCtbs) == 0)
                std::swap(saoFilter_New[compId].m_TmpU[0], saoFilter_New[compId].m_TmpU[1]);
        }
    }

    return MFX_ERR_NONE;
}

template <typename PixType>
mfxStatus H265Encoder::EncodeThread(Ipp32s ithread) {
    H265CU<PixType> *cu = (H265CU<PixType> *)this->cu;
    Ipp32u ctb_row = 0, ctb_col = 0, ctb_addr = 0;
    H265VideoParam *pars = &m_videoParam;
    Ipp8u nz[2];

    while(1) {
        mfxI32 found = 0, complete = 1;

        for (ctb_row = m_incRow; ctb_row < pars->PicHeightInCtbs; ctb_row++) {
            ctb_col = m_row_info[ctb_row].mt_current_ctb_col + 1;
            if (ctb_col == pars->PicWidthInCtbs)
                continue;
            complete = 0;
            if (m_row_info[ctb_row].mt_busy)
                continue;

            ctb_addr = ctb_row * pars->PicWidthInCtbs + ctb_col;

            if (pars->num_threads > 1 && m_pps.entropy_coding_sync_enabled_flag && ctb_row > 0) {
                Ipp32s nsync = ctb_col < pars->PicWidthInCtbs - 1 ? 1 : 0;
                if ((Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + nsync >= m_slices[m_slice_ids[ctb_addr]].slice_segment_address) {
                    if (m_row_info[ctb_row - 1].mt_current_ctb_col < (Ipp32s)ctb_col + nsync)
                        continue;
                }
            }

            mfxI32 locked = 0;
            if (pars->num_threads > 1 && m_pps.entropy_coding_sync_enabled_flag)
                locked = vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_busy)), 1, 0);

            if (locked)
                continue;

            if  ((Ipp32s)ctb_col != m_row_info[ctb_row].mt_current_ctb_col + 1) {
                m_row_info[ctb_row].mt_busy = 0;
                continue;
            }
            else {
                found = 1;
                break;
            }
        }

        if (complete)
            break;
        if (!found)
            continue;

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "EncodeThread");
        Ipp32s bs_id = 0;
        if (pars->num_threads > 1 && m_pps.entropy_coding_sync_enabled_flag)
            bs_id = ctb_row;

        {
            Ipp8u curr_slice = m_slice_ids[ctb_addr];
            Ipp8u end_of_slice_flag = (ctb_addr == m_slices[curr_slice].slice_address_last_ctb);

            if ((Ipp32s)ctb_addr == m_slices[curr_slice].slice_segment_address ||
                (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 0)) {

                ippiCABACInit_H265(&bs[ctb_row].cabacState,
                    bs[ctb_row].m_base.m_pbsBase,
                    bs[ctb_row].m_base.m_bitOffset + (Ipp32u)(bs[ctb_row].m_base.m_pbs - bs[ctb_row].m_base.m_pbsBase) * 8,
                    bs[ctb_row].m_base.m_maxBsSize);

                if (m_pps.entropy_coding_sync_enabled_flag && pars->PicWidthInCtbs > 1 &&
                    (Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + 1 >= m_slices[curr_slice].slice_segment_address) {
                    bs[ctb_row].CtxRestoreWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * (ctb_row - 1));
                } else {
                    InitializeContextVariablesHEVC_CABAC(bs[ctb_row].m_base.context_array, 2-m_slices[curr_slice].slice_type, pars->m_sliceQpY);
                }
            }

            small_memcpy(bsf[ctb_row].m_base.context_array, bs[ctb_row].m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);
            bsf[ctb_row].Reset();

#ifdef DEBUG_CABAC
            printf("\n");
            if (ctb_addr == 0) printf("Start POC %d\n",m_pCurrentFrame->m_PicOrderCnt);
            printf("CTB %d\n",ctb_addr);
#endif
            cu[ithread].InitCu(pars, m_pReconstructFrame->cu_data + (ctb_addr << pars->Log2NumPartInCU),
                data_temp + ithread * data_temp_size, ctb_addr, (PixType*)m_pReconstructFrame->y,
                (PixType*)m_pReconstructFrame->uv, m_pReconstructFrame->pitch_luma_pix, m_pCurrentFrame,
                &bsf[ctb_row], m_slices + curr_slice, 1, m_logMvCostTable);

            if(m_videoParam.UseDQP && m_videoParam.preEncMode) {
                int deltaQP = -(m_videoParam.m_sliceQpY - m_videoParam.m_lcuQps[ctb_addr]);
                int idxDqp = 2*abs(deltaQP)-((deltaQP<0)?1:0);

                H265Slice* curSlice = &(m_videoParam.m_dqpSlice[idxDqp]);
                cu[ithread].m_rdLambda = curSlice->rd_lambda_slice;
                cu[ithread]. m_rdLambdaSqrt = curSlice->rd_lambda_sqrt_slice;
                cu[ithread].m_ChromaDistWeight = curSlice->ChromaDistWeight_slice;
                cu[ithread].m_rdLambdaInter = curSlice->rd_lambda_inter_slice;
                cu[ithread].m_rdLambdaInterMv = curSlice->rd_lambda_inter_mv_slice;
                if(m_videoParam.preEncMode > 1)
                {

                    

                    int calq = cu[ithread].GetCalqDeltaQp(m_pAQP, m_slices[curr_slice].rd_lambda_slice);
                    m_videoParam.m_lcuQps[ctb_addr] += calq;
                }
            }
            
            cu[ithread].GetInitAvailablity();
            cu[ithread].ModeDecision(0, 0, 0, NULL);
            //cu[ithread].FillRandom(0, 0);
            //cu[ithread].FillZero(0, 0);

            if (pars->RDOQFlag )
                small_memcpy(bsf[ctb_row].m_base.context_array, bs[ctb_row].m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);

            // inter pred for chroma is now performed in EncAndRecLuma
            cu[ithread].EncAndRecLuma(0, 0, 0, nz, NULL);
            cu[ithread].EncAndRecChroma(0, 0, 0, nz, NULL);

            if (!(m_slices[curr_slice].slice_deblocking_filter_disabled_flag) &&
                (pars->num_threads == 1 || m_sps.sample_adaptive_offset_enabled_flag))
            {
#if defined(MFX_HEVC_SAO_PREDEBLOCKED_ENABLED)
                if (m_sps.sample_adaptive_offset_enabled_flag)
                {
                    Ipp32s left_addr  = cu[ithread].left_addr;
                    Ipp32s above_addr = cu[ithread].above_addr;
                    MFX_HEVC_PP::CTBBorders borders = {0};

                    borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                    borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

                    cu[ithread].GetStatisticsCtuSaoPredeblocked( borders );
                }
#endif
                if (pars->UseDQP)
                    cu[ithread].UpdateCuQp();
            
                cu[ithread].Deblock();
                if (m_sps.sample_adaptive_offset_enabled_flag) {
                    Ipp32s left_addr = cu[ithread].m_leftAddr;
                    Ipp32s above_addr = cu[ithread].m_aboveAddr;

                    MFX_HEVC_PP::CTBBorders borders = {0};

                    borders.m_left  = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                    borders.m_top = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

                    // aya: here should be slice lambda always
                    cu[ithread].m_rdLambda = m_slices[curr_slice].rd_lambda_slice;
					
                    cu[ithread].EstimateCtuSao(&bsf[ctb_row], &m_saoParam[ctb_addr],
                                               &m_saoParam[0], borders, m_slice_ids);

                    // aya: tiles issues???
                    borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[left_addr]) ? 1 : 0;
                    borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[above_addr]) ? 1 : 0;

                    bool leftMergeAvail  = borders.m_left > 0 ? true : false;
                    bool aboveMergeAvail = borders.m_top  > 0 ? true : false;

                    cu[ithread].EncodeSao(&bs[ctb_row], 0, 0, 0, m_saoParam[ctb_addr],
                                          leftMergeAvail, aboveMergeAvail);

                    cu[ithread].m_saoEncodeFilter.ReconstructCtuSaoParam(m_saoParam[ctb_addr]);
                }
            }

            cu[ithread].EncodeCU(&bs[ctb_row], 0, 0, 0);

            if (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 1)
                bs[ctb_row].CtxSaveWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * ctb_row);

            bs[ctb_row].EncodeSingleBin_CABAC(CTX(&bs[ctb_row],END_OF_SLICE_FLAG_HEVC), end_of_slice_flag);

            if ((m_pps.entropy_coding_sync_enabled_flag && ctb_col == pars->PicWidthInCtbs - 1) ||
                end_of_slice_flag)
            {
#ifdef DEBUG_CABAC
                int d = DEBUG_CABAC_PRINT;
                DEBUG_CABAC_PRINT = 1;
#endif
                if (!end_of_slice_flag)
                    bs[ctb_row].EncodeSingleBin_CABAC(CTX(&bs[ctb_row],END_OF_SLICE_FLAG_HEVC), 1);
                bs[ctb_row].TerminateEncode_CABAC();
                bs[ctb_row].ByteAlignWithZeros();
#ifdef DEBUG_CABAC
                DEBUG_CABAC_PRINT = d;
#endif
            }
            m_row_info[ctb_row].mt_current_ctb_col = ctb_col;
            if (ctb_row == m_incRow && ctb_col == pars->PicWidthInCtbs - 1)
                m_incRow++;
            m_row_info[ctb_row].mt_busy = 0;
        }
    }
    return MFX_ERR_NONE;
}

template <typename PixType>
mfxStatus H265Encoder::EncodeThreadByRow(Ipp32s ithread) {
    H265CU<PixType> *cu = (H265CU<PixType> *)this->cu;
    Ipp32u ctb_row;
    H265VideoParam *pars = &m_videoParam;
    Ipp8u nz[2];
    Ipp32s offset = 0;
    H265EncoderRowInfo *row_info = NULL;

    while(1) {
        if (pars->num_threads > 1)
            ctb_row = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&m_incRow)) - 1;
        else
            ctb_row = m_incRow++;

        if (ctb_row >= pars->PicHeightInCtbs)
            break;

        Ipp32u ctb_addr = ctb_row * pars->PicWidthInCtbs;

        for (Ipp32u ctb_col = 0; ctb_col < pars->PicWidthInCtbs; ctb_col ++, ctb_addr++) {
            Ipp8u curr_slice = m_slice_ids[ctb_addr];
            Ipp8u end_of_slice_flag = (ctb_addr == m_slices[curr_slice].slice_address_last_ctb);

            if (pars->num_threads > 1 && m_pps.entropy_coding_sync_enabled_flag && ctb_row > 0) {
                Ipp32s nsync = ctb_col < pars->PicWidthInCtbs - 1 ? 1 : 0;
                if ((Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + nsync >= m_slices[m_slice_ids[ctb_addr]].slice_segment_address) {
                    while(m_row_info[ctb_row - 1].mt_current_ctb_col < (Ipp32s)ctb_col + nsync) {
                        x86_pause();
                        thread_sleep(0);
                    }
                }
            }

            if ((Ipp32s)ctb_addr == m_slices[curr_slice].slice_segment_address ||
                (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 0))
            {
                ippiCABACInit_H265(&bs[ithread].cabacState,
                    bs[ithread].m_base.m_pbsBase,
                    bs[ithread].m_base.m_bitOffset + (Ipp32u)(bs[ithread].m_base.m_pbs - bs[ithread].m_base.m_pbsBase) * 8,
                    bs[ithread].m_base.m_maxBsSize);

                if (m_pps.entropy_coding_sync_enabled_flag && pars->PicWidthInCtbs > 1 &&
                    (Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + 1 >= m_slices[curr_slice].slice_segment_address) {
                    bs[ithread].CtxRestoreWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * (ctb_row - 1));
                } else {
                    InitializeContextVariablesHEVC_CABAC(bs[ithread].m_base.context_array, 2-m_slices[curr_slice].slice_type, pars->m_sliceQpY);
                }

                if ((Ipp32s)ctb_addr == m_slices[curr_slice].slice_segment_address)
                    row_info = &(m_slices[curr_slice].m_row_info);
                else
                    row_info = m_row_info + ctb_row;
                row_info->offset = offset = H265Bs_GetBsSize(&bs[ithread]);
            }

            small_memcpy(bsf[ithread].m_base.context_array, bs[ithread].m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);
            bsf[ithread].Reset();

#ifdef DEBUG_CABAC
            printf("\n");
            if (ctb_addr == 0) printf("Start POC %d\n",m_pCurrentFrame->m_PicOrderCnt);
            printf("CTB %d\n",ctb_addr);
#endif
            cu[ithread].InitCu(pars, m_pReconstructFrame->cu_data + (ctb_addr << pars->Log2NumPartInCU),
                data_temp + ithread * data_temp_size, ctb_addr, (PixType*)m_pReconstructFrame->y,
                (PixType*)m_pReconstructFrame->uv, m_pReconstructFrame->pitch_luma_pix, m_pCurrentFrame,
                &bsf[ithread], m_slices + curr_slice, 1, m_logMvCostTable);

            // lambda_Ctb = F(QP_Ctb)
            if(m_videoParam.UseDQP && m_videoParam.preEncMode)
            {
                int deltaQP = -(m_videoParam.m_sliceQpY - m_videoParam.m_lcuQps[ctb_addr]);
                int idxDqp = 2*abs(deltaQP)-((deltaQP<0)?1:0);

                H265Slice* curSlice = &(m_videoParam.m_dqpSlice[idxDqp]);

                cu[ithread].m_rdLambda = curSlice->rd_lambda_slice;
                cu[ithread]. m_rdLambdaSqrt = curSlice->rd_lambda_sqrt_slice;
                cu[ithread].m_ChromaDistWeight = curSlice->ChromaDistWeight_slice;
                cu[ithread].m_rdLambdaInter = curSlice->rd_lambda_inter_slice;
                cu[ithread].m_rdLambdaInterMv = curSlice->rd_lambda_inter_mv_slice;
                if(m_videoParam.preEncMode > 1) {
                    int calq = cu[ithread].GetCalqDeltaQp(m_pAQP, m_slices[curr_slice].rd_lambda_slice);
                    m_videoParam.m_lcuQps[ctb_addr] += calq;
                }
            }
            
            cu[ithread].GetInitAvailablity();
            cu[ithread].ModeDecision(0, 0, 0, NULL);
            //cu[ithread].FillRandom(0, 0);
            //cu[ithread].FillZero(0, 0);

            if (pars->RDOQFlag )
                small_memcpy(bsf[ithread].m_base.context_array, bs[ithread].m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);

            // inter pred for chroma is now performed in EncAndRecLuma
            cu[ithread].EncAndRecLuma(0, 0, 0, nz, NULL);
            cu[ithread].EncAndRecChroma(0, 0, 0, nz, NULL);

            if (!(m_slices[curr_slice].slice_deblocking_filter_disabled_flag) &&
                (pars->num_threads == 1 || m_sps.sample_adaptive_offset_enabled_flag))
            {
#if defined(MFX_HEVC_SAO_PREDEBLOCKED_ENABLED)
                if (m_sps.sample_adaptive_offset_enabled_flag) {
                    Ipp32s left_addr  = cu[ithread].left_addr;
                    Ipp32s above_addr = cu[ithread].above_addr;
                    MFX_HEVC_PP::CTBBorders borders = {0};

                    borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                    borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

                    cu[ithread].GetStatisticsCtuSaoPredeblocked( borders );
                }
#endif
                if (pars->UseDQP)
                    cu[ithread].UpdateCuQp();

                cu[ithread].Deblock();
                if (m_sps.sample_adaptive_offset_enabled_flag) {
                    Ipp32s left_addr  = cu[ithread].m_leftAddr;
                    Ipp32s above_addr = cu[ithread].m_aboveAddr;
                    MFX_HEVC_PP::CTBBorders borders = {0};

                    borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                    borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

                    // aya: here should be slice lambda always
                    cu[ithread].m_rdLambda = m_slices[curr_slice].rd_lambda_slice;
                    cu[ithread].EstimateCtuSao(&bsf[ithread], &m_saoParam[ctb_addr],
                                               ctb_addr > 0 ? &m_saoParam[0] : NULL,
                                               borders, m_slice_ids);

                    // aya: tiles issues???
                    borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[left_addr])  ? 1 : 0;
                    borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[above_addr]) ? 1 : 0;

                    bool leftMergeAvail = borders.m_left > 0 ? true : false;
                    bool aboveMergeAvail= borders.m_top > 0 ? true : false;

                    cu[ithread].EncodeSao(&bs[ithread], 0, 0, 0, m_saoParam[ctb_addr],
                                          leftMergeAvail, aboveMergeAvail);

                    cu[ithread].m_saoEncodeFilter.ReconstructCtuSaoParam(m_saoParam[ctb_addr]);
                }
            }

            cu[ithread].EncodeCU(&bs[ithread], 0, 0, 0);

            if (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 1)
                bs[ithread].CtxSaveWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * ctb_row);

            bs[ithread].EncodeSingleBin_CABAC(CTX(&bs[ithread],END_OF_SLICE_FLAG_HEVC), end_of_slice_flag);

            if ((m_pps.entropy_coding_sync_enabled_flag && ctb_col == pars->PicWidthInCtbs - 1) ||
                end_of_slice_flag)
            {
#ifdef DEBUG_CABAC
                int d = DEBUG_CABAC_PRINT;
                DEBUG_CABAC_PRINT = 1;
#endif
                if (!end_of_slice_flag)
                    bs[ithread].EncodeSingleBin_CABAC(CTX(&bs[ithread],END_OF_SLICE_FLAG_HEVC), 1);
                bs[ithread].TerminateEncode_CABAC();
                bs[ithread].ByteAlignWithZeros();
#ifdef DEBUG_CABAC
                DEBUG_CABAC_PRINT = d;
#endif
                row_info->bs_id = ithread;
                row_info->size = H265Bs_GetBsSize(&bs[ithread]) - offset;
            }
            m_row_info[ctb_row].mt_current_ctb_col = ctb_col;
        }
    }
    return MFX_ERR_NONE;
}

static Ipp8u h265_pgop_qp_diff[PGOP_PIC_SIZE] = {0, 2, 1, 2};

void H265Encoder::PrepareToEndSequence()
{
    if (m_pLastFrame && m_pLastFrame->m_PicCodType == MFX_FRAMETYPE_B) {
        if (m_videoParam.BPyramid) {
            H265Frame *pCurr = m_cpb.head();
            Ipp8s tmbBuff[129];

            for (Ipp32s i = 0; i < 129; i++)
                tmbBuff[i] = -1;

            while (pCurr) {
                if (pCurr->m_PicCodType == MFX_FRAMETYPE_B) {
                    Ipp32u active_L1_refs;
                    m_dpb.countL1Refs(active_L1_refs, pCurr->PicOrderCnt());

                    if (active_L1_refs == 0)
                        tmbBuff[pCurr->m_BpyramidNumFrame] = 1;
                }
                pCurr = pCurr->future();
            }

            pCurr = m_cpb.head();

            while (pCurr) {
                if (pCurr->m_PicCodType == MFX_FRAMETYPE_B) {
                    Ipp32u active_L1_refs;
                    m_dpb.countL1Refs(active_L1_refs, pCurr->PicOrderCnt());

                    if (active_L1_refs == 0)
                        if (tmbBuff[m_BpyramidTabRight[pCurr->m_BpyramidNumFrame]] != 1)
                            pCurr->m_PicCodType = MFX_FRAMETYPE_P;
                }
                pCurr = pCurr->future();
            }
        }
    }
}

mfxStatus H265Encoder::EncodeFrame(mfxFrameSurface1 *surface, mfxBitstream *mfxBS)
{
    EnumPicClass ePic_Class;

    if (surface) {
        //Ipp32s RPSIndex = m_iProfileIndex;
        Ipp32u  ePictureType;

        ePictureType = DetermineFrameType();
        m_frameCountSend++;   // is used to determine 1st I frame
        mfxU64 prevTimeStamp = m_pLastFrame ? m_pLastFrame->TimeStamp : MFX_TIMESTAMP_UNKNOWN;
        /*m_pLastFrame = */m_pCurrentFrame = m_cpb.InsertFrame(surface, &m_videoParam);
        if (m_pCurrentFrame) {
#ifdef MFX_ENABLE_WATERMARK
            m_watermark->Apply(m_pCurrentFrame->y, m_pCurrentFrame->uv, m_pCurrentFrame->pitch_luma,
                surface->Info.Width, surface->Info.Height);
#endif
            // Set PTS  from previous if isn't given at input
            if (m_pCurrentFrame->TimeStamp == MFX_TIMESTAMP_UNKNOWN) {
                if (prevTimeStamp == MFX_TIMESTAMP_UNKNOWN) {
                    m_pCurrentFrame->TimeStamp = 0;
                }
                else {
                    mfxF64 tcDuration90KHz = (mfxF64)m_videoParam.FrameRateExtD / m_videoParam.FrameRateExtN * 90000; // calculate tick duration
                    m_pCurrentFrame->TimeStamp = mfxI64(prevTimeStamp + tcDuration90KHz);
                }
            }

            m_pCurrentFrame->m_PicCodType = ePictureType;
            m_pCurrentFrame->m_bIsIDRPic = m_bMakeNextFrameIDR;
            m_pCurrentFrame->m_RPSIndex = m_iProfileIndex;

            // making IDR picture every I frame for SEI HRD_CONF
            if (//((m_info.EnableSEI) && ePictureType == INTRAPIC) ||
               (/*(!m_info.EnableSEI) && */m_bMakeNextFrameIDR /*&& ePictureType == MFX_FRAMETYPE_I*/))
            {
                //m_pCurrentFrame->m_bIsIDRPic = true;
                m_PicOrderCnt_Accu += m_PicOrderCnt;
                m_PicOrderCnt = 0;
                //m_bMakeNextFrameIDR = false;
                m_PGOPIndex = 0;
            }

            m_pCurrentFrame->m_PGOPIndex = m_PGOPIndex;
            if (m_iProfileIndex == 0) {
                m_PGOPIndex++;
                if (m_PGOPIndex == m_videoParam.PGopPicSize)
                    m_PGOPIndex = 0;
            }

            m_pCurrentFrame->setPicOrderCnt(m_PicOrderCnt);
            m_pCurrentFrame->m_PicOrderCounterAccumulated = m_PicOrderCnt_Accu; //(m_PicOrderCnt + m_PicOrderCnt_Accu);

            m_pCurrentFrame->InitRefPicListResetCount();

            m_PicOrderCnt++;

            if (m_videoParam.BPyramid) {
                if (ePictureType == MFX_FRAMETYPE_I || ePictureType == MFX_FRAMETYPE_P)
                    m_Bpyramid_currentNumFrame = 0;

                m_pCurrentFrame->m_BpyramidNumFrame = m_BpyramidTab[m_Bpyramid_currentNumFrame];
                m_pCurrentFrame->m_isBRef = ((m_Bpyramid_currentNumFrame & 1) == 0) ? 1 : 0;
                m_Bpyramid_currentNumFrame++;
                if (m_Bpyramid_currentNumFrame == m_Bpyramid_maxNumFrame)
                    m_Bpyramid_currentNumFrame = 0;

                m_l1_cnt_to_start_B = 1;
            }

            if (m_pCurrentFrame->m_bIsIDRPic) {
                m_cpb.IncreaseRefPicListResetCount(m_pCurrentFrame);
                if (m_videoParam.BPyramid) {
                    PrepareToEndSequence();
                }
                else if (m_pLastFrame && !m_pLastFrame->wasEncoded() &&
                         m_pLastFrame->m_PicCodType == MFX_FRAMETYPE_B)
                {
                    if (!m_videoParam.GopStrictFlag)
                        m_pLastFrame->m_PicCodType = MFX_FRAMETYPE_P;
                    //else 
                    //    MoveTrailingBtoNextGOP(); // Correct m_PicOrderCnt
                }
            }

            m_pLastFrame = m_pCurrentFrame;
        }
        else {
            m_pLastFrame = m_pCurrentFrame;
            return MFX_ERR_UNKNOWN;
        }
    }
    else {
        if (m_videoParam.BPyramid) {
            PrepareToEndSequence();
        }
        else {
            if (m_pLastFrame && !m_pLastFrame->wasEncoded() &&
                m_pLastFrame->m_PicCodType == MFX_FRAMETYPE_B && !m_videoParam.GopStrictFlag)
            {
                m_pLastFrame->m_PicCodType = MFX_FRAMETYPE_P;
            }
        }
    }

    m_pCurrentFrame = m_cpb.findOldestToEncode(&m_dpb, m_l1_cnt_to_start_B,
                                               m_videoParam.BPyramid, m_Bpyramid_nextNumFrame);

    if (m_l1_cnt_to_start_B == 0 && m_pCurrentFrame &&
        m_pCurrentFrame->m_PicCodType == MFX_FRAMETYPE_B && !m_videoParam.GopStrictFlag)
    {
        m_pCurrentFrame->m_PicCodType = MFX_FRAMETYPE_P;
    }

    // make sure no B pic is left unencoded before program exits
    if (!m_cpb.isEmpty() && !m_pCurrentFrame && !surface) {
        m_pCurrentFrame = m_cpb.findOldestToEncode(&m_dpb, 0, 0, 0/*m_isBpyramid, m_Bpyramid_nextNumFrame*/);
        if (!m_videoParam.BPyramid && m_pCurrentFrame && m_pCurrentFrame->m_PicCodType == MFX_FRAMETYPE_B && !m_videoParam.GopStrictFlag)
            m_pCurrentFrame->m_PicCodType = MFX_FRAMETYPE_P;
    }

    // buffering here
    if( 0 == m_videoParam.preEncMode) {
        if (!mfxBS) {
            return m_pCurrentFrame ? MFX_ERR_NONE : MFX_ERR_MORE_DATA;
        }
    } else {
        mfxStatus sts = PreEncAnalysis(mfxBS);
        if (!mfxBS)  {
            return m_pCurrentFrame ? MFX_ERR_NONE : MFX_ERR_MORE_DATA;
        }        
        MFX_CHECK_STS(sts);
    }

    // start normal way
    Ipp32u ePictureType;

    if (m_pCurrentFrame) {
        ePictureType = m_pCurrentFrame->m_PicCodType;
        MoveFromCPBToDPB();
        if (m_videoParam.BPyramid) {
            m_Bpyramid_nextNumFrame = m_pCurrentFrame->m_BpyramidNumFrame + 1;
            if (m_Bpyramid_nextNumFrame == m_Bpyramid_maxNumFrame)
                m_Bpyramid_nextNumFrame = 0;
        }
    }
    else {
        return MFX_ERR_MORE_DATA;
    }


    ePictureType = m_pCurrentFrame->m_PicCodType;
    // Determine the Pic_Class.  Right now this depends on ePictureType, but that could change
    // to permit disposable P frames, for example.
    switch (ePictureType)
    {
    case MFX_FRAMETYPE_I:
        m_videoParam.m_sliceQpY = m_videoParam.QPI;
        if (m_videoParam.GopPicSize == 1) {
             ePic_Class = DISPOSABLE_PIC;
        }
        else if (m_pCurrentFrame->m_bIsIDRPic) {
            if (m_frameCountEncoded) {
                m_dpb.unMarkAll();
                m_dpb.removeAllUnmarkedRef();
            }
            ePic_Class = IDR_PIC;
            m_l1_cnt_to_start_B = m_videoParam.NumRefToStartCodeBSlice;
        }
        else {
            ePic_Class = REFERENCE_PIC;
        }
        break;

    case MFX_FRAMETYPE_P:
         m_videoParam.m_sliceQpY = m_videoParam.QPP;
         if (m_videoParam.PGopPicSize > 1)
            m_videoParam.m_sliceQpY += h265_pgop_qp_diff[m_pCurrentFrame->m_PGOPIndex];
        
        ePic_Class = REFERENCE_PIC;
        break;

    case MFX_FRAMETYPE_B:
        m_videoParam.m_sliceQpY = m_videoParam.QPB;
        if (m_videoParam.BPyramid)
            m_videoParam.m_sliceQpY += m_BpyramidRefLayers[m_pCurrentFrame->m_PicOrderCnt % m_videoParam.GopRefDist];
        
        if (!m_videoParam.BPyramid)
        {
            ePic_Class = m_videoParam.TreatBAsReference ? REFERENCE_PIC : DISPOSABLE_PIC;
        }
        else
        {
            ePic_Class = (m_pCurrentFrame->m_isBRef == 1) ? REFERENCE_PIC : DISPOSABLE_PIC;
        }
        break;

    default:
        // Unsupported Picture Type
        VM_ASSERT(false);
        m_videoParam.m_sliceQpY = m_videoParam.QPI;
        
        ePic_Class = IDR_PIC;
        break;
    }

    Ipp32s numCtb = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;
    memset(m_videoParam.m_lcuQps, m_videoParam.m_sliceQpY, sizeof(m_videoParam.m_sliceQpY)*numCtb);

    H265NALUnit nal;
    nal.nuh_layer_id = 0;
    nal.nuh_temporal_id = 0;

    mfxU32 initialDatalength = mfxBS->DataLength;
    Ipp32s overheadBytes = 0;
    Ipp32s bs_main_id = m_videoParam.num_thread_structs;

    if (m_frameCountEncoded == 0) {
        bs[bs_main_id].Reset();
        PutVPS(&bs[bs_main_id]);
        bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_UT_VPS;
        overheadBytes += bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
    }

    if (m_pCurrentFrame->m_bIsIDRPic) {
        bs[bs_main_id].Reset();

        PutSPS(&bs[bs_main_id]);
        bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_UT_SPS;
        bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);

        PutPPS(&bs[bs_main_id]);
        bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_UT_PPS;
        overheadBytes += bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
    }

    Ipp8u *pbs0;
    Ipp32u bitOffset0;
    Ipp32s overheadBytes0 = overheadBytes;
    H265Bs_GetState(&bs[bs_main_id], &pbs0, &bitOffset0);
    Ipp32s min_qp = 1;
    Ipp32u dataLength0 = mfxBS->DataLength;
    Ipp32s brcRecode = 0;

recode:

    if (m_brc) {
        m_videoParam.m_sliceQpY = (Ipp8s)m_brc->GetQP((mfxU16)ePictureType);
        memset(m_videoParam.m_lcuQps, m_videoParam.m_sliceQpY, sizeof(m_videoParam.m_sliceQpY)*numCtb);
    }

#if defined (MFX_ENABLE_H265_PAQ)
    if(m_videoParam.preEncMode && m_videoParam.UseDQP)
    {
        // GetDeltaQp();
        //set m_videoParam.m_lcuQps[addr = 0:N]
        int poc = m_pCurrentFrame->PicOrderCnt();
        int frameDeltaQP = 0;
        for(int ctb=0; ctb<numCtb; ctb++)
        {
            int ctb_adapt = ctb;
            if(32 == m_videoParam.MaxCUSize) {
                int ctuX = ctb % m_videoParam.PicWidthInCtbs;
                int ctuY = ctb / m_videoParam.PicWidthInCtbs;

                ctb_adapt = (ctuX >> 1) + (ctuY >> 1)*(m_videoParam.PicWidthInCtbs >> 1);
            }

            int locPoc = poc % m_preEnc.m_histLength;
            if(m_videoParam.preEncMode != 2) // no pure CALQ
            {
                m_videoParam.m_lcuQps[ctb] += m_preEnc.m_acQPMap[locPoc].getDQP(ctb_adapt);
            }
            frameDeltaQP += m_preEnc.m_acQPMap[locPoc].getDQP(ctb_adapt);
        }
    }
#endif

    for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
        H265Slice *slice = m_slices + curr_slice;
        SetSlice(slice, curr_slice);
#if defined (MFX_ENABLE_H265_PAQ)
        if(m_videoParam.preEncMode && m_videoParam.UseDQP)  {
            UpdateAllLambda();
        }
#endif
      
        if (CheckRefPicSet(slice, m_pCurrentFrame) != MFX_ERR_NONE)
            CreateRefPicSet(slice, m_pCurrentFrame);

        UpdateRefPicList(slice, m_pCurrentFrame);

        if (slice->slice_type != I_SLICE) {
            slice->num_ref_idx[0] = slice->num_ref_idx_l0_active;
            slice->num_ref_idx[1] = slice->num_ref_idx_l1_active;
        }
        else {
            slice->num_ref_idx[0] = 0;
            slice->num_ref_idx[1] = 0;
        }

        for (Ipp32u i = slice->slice_segment_address; i <= slice->slice_address_last_ctb; i++)
            m_slice_ids[i] = curr_slice;

        H265Frame **list0 = m_pCurrentFrame->m_refPicList[0].m_refFrames;
        H265Frame **list1 = m_pCurrentFrame->m_refPicList[1].m_refFrames;
        for (Ipp32s idx1 = 0; idx1 < slice->num_ref_idx[1]; idx1++) {
            Ipp32s idx0 = (Ipp32s)(std::find(list0, list0 + slice->num_ref_idx[0], list1[idx1]) - list0);
            m_pCurrentFrame->m_mapRefIdxL1ToL0[idx1] = (idx0 < slice->num_ref_idx[0]) ? idx0 : -1;
        }

        m_pCurrentFrame->m_allRefFramesAreFromThePast = true;
        for (Ipp32s i = 0; i < slice->num_ref_idx[0] && m_pCurrentFrame->m_allRefFramesAreFromThePast; i++)
            if (m_pCurrentFrame->m_refPicList[0].m_deltaPoc[i] < 0)
                m_pCurrentFrame->m_allRefFramesAreFromThePast = false;
        for (Ipp32s i = 0; i < slice->num_ref_idx[1] && m_pCurrentFrame->m_allRefFramesAreFromThePast; i++)
            if (m_pCurrentFrame->m_refPicList[1].m_deltaPoc[i] < 0)
                m_pCurrentFrame->m_allRefFramesAreFromThePast = false;
    }

    m_incRow = 0;
    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs; i++)
        bs[i].Reset();
    for (Ipp32u i = 0; i < m_videoParam.PicHeightInCtbs; i++) {
        m_row_info[i].mt_current_ctb_col = -1;
        m_row_info[i].mt_busy = 1;
    }

    if (m_videoParam.num_threads > 1)
        mfx_video_encode_h265_ptr->ParallelRegionStart(m_videoParam.num_threads - 1, PARALLEL_REGION_MAIN);

#if defined (MFX_ENABLE_CM)
    if (m_videoParam.enableCmFlag) {
        cmCurIdx ^= 1;
        cmNextIdx ^= 1;

        m_pCurrentFrame->setEncOrderNum(m_frameCountEncoded);
#if 0
        printf("VME Curr poc = %i type = %i (0 - B, 1- P, 2 - I)\n",  m_pCurrentFrame->PicOrderCnt(), m_slices->slice_type);
        printf("L0 pocs: ");
        H265Frame **pFrames = m_pCurrentFrame->m_refPicList[0].m_refFrames;
        for (int ii = 0; ii < m_slices->num_ref_idx[0]; ii++) {
            printf("%i ", pFrames[ii]->PicOrderCnt());
        }
        printf("\tL1 pocs: ");
        pFrames = m_pCurrentFrame->m_refPicList[1].m_refFrames;
        for (int ii = 0; ii < m_slices->num_ref_idx[1]; ii++) {
            printf("%i ", pFrames[ii]->PicOrderCnt());
        }
        printf("\t\t");
        printf("DPB pocs: ");
        H265Frame *pFr = m_dpb.head();
        while (pFr) {
            printf("%i ", pFr->PicOrderCnt());
            pFr = pFr->future();
        }
        printf("\n");
#endif
        RunVmeCurr(m_videoParam, m_pCurrentFrame, m_slices, &m_dpb);
    }
#endif // MFX_ENABLE_CM

    for (Ipp32u i = 0; i < m_videoParam.PicHeightInCtbs; i++)
        m_row_info[i].mt_busy = 0;

#if defined (MFX_ENABLE_CM)
    if (m_videoParam.enableCmFlag) {

        switch (ePic_Class)
        {
        case IDR_PIC:
        case REFERENCE_PIC:
            m_pCurrentFrame->SetisShortTermRef();
            break;
        case DISPOSABLE_PIC:
        default:
            break;
        }

        // m_Bpyramid_nextNumFrame is already updated above
        m_pNextFrame = m_cpb.findOldestToEncode(&m_dpb, m_l1_cnt_to_start_B, m_videoParam.BPyramid, m_Bpyramid_nextNumFrame);
        if (m_pNextFrame)
        {
            m_pCurrentFrame->setWasEncoded();   // to make it disposable in ref list for Next

            CleanDPB(); // disposable frames should be removed for correct ref lists

            small_memcpy(&m_ShortRefPicSetDump, &m_ShortRefPicSet, sizeof(m_ShortRefPicSet)); // !!!TODO: not all should be saved sergo!!!
            for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
                H265Slice *pSlice = m_slicesNext + curr_slice;
                SetSlice(pSlice, curr_slice, m_pNextFrame);
                if (CheckRefPicSet(pSlice, m_pNextFrame) != MFX_ERR_NONE)
                    CreateRefPicSet(pSlice, m_pNextFrame);
                UpdateRefPicList(pSlice, m_pNextFrame);
                if (pSlice->slice_type != I_SLICE) {
                    pSlice->num_ref_idx[0] = pSlice->num_ref_idx_l0_active;
                    pSlice->num_ref_idx[1] = pSlice->num_ref_idx_l1_active;
                }
                else {
                    pSlice->num_ref_idx[0] = 0;
                    pSlice->num_ref_idx[1] = 0;
                }
                for (Ipp32u i = m_slicesNext[curr_slice].slice_segment_address; i <= m_slicesNext[curr_slice].slice_address_last_ctb; i++)
                    m_slice_ids[i] = curr_slice;
            }
            small_memcpy(&m_ShortRefPicSet, &m_ShortRefPicSetDump, sizeof(m_ShortRefPicSet));
            m_pNextFrame->setEncOrderNum(m_pCurrentFrame->EncOrderNum() + 1);

            H265Frame **list0 = m_pNextFrame->m_refPicList[0].m_refFrames;
            H265Frame **list1 = m_pNextFrame->m_refPicList[1].m_refFrames;
            for (Ipp32s idx1 = 0; idx1 < m_slicesNext->num_ref_idx[1]; idx1++) {
                Ipp32s idx0 = (Ipp32s)(std::find(list0, list0 + m_slicesNext->num_ref_idx[0], list1[idx1]) - list0);
                m_pNextFrame->m_mapRefIdxL1ToL0[idx1] = (idx0 < m_slicesNext->num_ref_idx[0]) ? idx0 : -1;
            }
        }
#if 0
        if (m_pNextFrame) {
            printf("VME Next poc = %i type = %i (0 - B, 1- P, 2 - I)\n",  m_pNextFrame->PicOrderCnt(), m_slicesNext->slice_type);
            printf("L0 pocs: ");
            H265Frame **pFrames = m_pNextFrame->m_refPicList[0].m_refFrames;
            for (int ii = 0; ii < m_slicesNext->num_ref_idx[0]; ii++) {
                printf("%i ", pFrames[ii]->PicOrderCnt());
            }
            printf("\tL1 pocs: ");
            pFrames = m_pNextFrame->m_refPicList[1].m_refFrames;
            for (int ii = 0; ii < m_slicesNext->num_ref_idx[1]; ii++) {
                printf("%i ", pFrames[ii]->PicOrderCnt());
            }
            printf("\t\t");
            printf("DPB pocs: ");
            H265Frame *pFr = m_dpb.head();
            while (pFr) {
                printf("%i ", pFr->PicOrderCnt());
                pFr = pFr->future();
            }
            printf("\n");
        }
#endif
        RunVmeNext(m_videoParam, m_pNextFrame, m_slicesNext, &m_dpb);
    }
#endif // MFX_ENABLE_CM

    if (m_videoParam.threading_by_rows)
        m_videoParam.bitDepthLuma == 8 ? EncodeThreadByRow<Ipp8u>(0) : EncodeThreadByRow<Ipp16u>(0);
    else
        m_videoParam.bitDepthLuma == 8 ? EncodeThread<Ipp8u>(0) : EncodeThread<Ipp16u>(0);

    if (m_videoParam.num_threads > 1)
        mfx_video_encode_h265_ptr->ParallelRegionEnd();

    if ((!m_pps.pps_deblocking_filter_disabled_flag) &&
        (m_videoParam.num_threads > 1 && !m_sps.sample_adaptive_offset_enabled_flag))
    {
        m_incRow = 0;
        mfx_video_encode_h265_ptr->ParallelRegionStart(m_videoParam.num_threads - 1, PARALLEL_REGION_DEBLOCKING);
        m_videoParam.bitDepthLuma == 8 ? DeblockThread<Ipp8u>(0) : DeblockThread<Ipp16u>(0);
        mfx_video_encode_h265_ptr->ParallelRegionEnd();
    }

    if (m_sps.sample_adaptive_offset_enabled_flag)
        m_videoParam.bitDepthLuma == 8 ? ApplySAOThread<Ipp8u>(0) : ApplySAOThread<Ipp16u>(0);

    if (m_videoParam.threading_by_rows) {
        for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
            H265Slice *pSlice = m_slices + curr_slice;
            H265EncoderRowInfo *row_info;

            row_info = &pSlice->m_row_info;
            if (m_pps.entropy_coding_sync_enabled_flag) {
                for (Ipp32u i = 0; i < pSlice->num_entry_point_offsets; i++) {
                    Ipp32u offset_add = 0;
                    Ipp8u *curPtr = bs[row_info->bs_id].m_base.m_pbsBase + row_info->offset;

                    for (Ipp32s j = 1; j < row_info->size - 1; j++) {
                        if (!curPtr[j - 1] && !curPtr[j] && !(curPtr[j + 1] & 0xfc)) {
                            j++;
                            offset_add ++;
                        }
                    }

                    pSlice->entry_point_offset[i] = row_info->size + offset_add;
                    row_info = m_row_info + pSlice->row_first + i + 1;
                }
                row_info = &pSlice->m_row_info;
            }

            bs[bs_main_id].Reset();
            PutSliceHeader(&bs[bs_main_id], pSlice);
            overheadBytes += H265Bs_GetBsSize(&bs[bs_main_id]);
            if (m_pps.entropy_coding_sync_enabled_flag) {
                for (Ipp32u row = pSlice->row_first; row <= pSlice->row_last; row++) {
                    Ipp32s ithread = row_info->bs_id;
                    small_memcpy(bs[bs_main_id].m_base.m_pbs, bs[ithread].m_base.m_pbsBase + row_info->offset, row_info->size);
                    bs[bs_main_id].m_base.m_pbs += row_info->size;
                    row_info = m_row_info + row + 1;
                }
            }
            else {
                small_memcpy(bs[bs_main_id].m_base.m_pbs, bs[row_info->bs_id].m_base.m_pbsBase + row_info->offset, row_info->size);
                bs[bs_main_id].m_base.m_pbs += row_info->size;
            }
            nal.nal_unit_type = (Ipp8u)(pSlice->IdrPicFlag ? NAL_UT_CODED_SLICE_IDR :
                (m_pCurrentFrame->PicOrderCnt() >= 0 ? NAL_UT_CODED_SLICE_TRAIL_R :
                    (m_pCurrentFrame->m_isBRef ? NAL_UT_CODED_SLICE_DLP : NAL_UT_CODED_SLICE_RADL_N)));
            bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
        }
    }
    else {
        for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
            H265Slice *pSlice = m_slices + curr_slice;
            Ipp32s ctb_row = pSlice->row_first;

            if (m_pps.entropy_coding_sync_enabled_flag) {
                for (Ipp32u i = 0; i < pSlice->num_entry_point_offsets; i++) {
                    Ipp32u offset_add = 0;
                    Ipp8u *curPtr = bs[ctb_row].m_base.m_pbsBase;
                    Ipp32s size = H265Bs_GetBsSize(&bs[ctb_row]);

                    for (Ipp32s j = 1; j < size - 1; j++) {
                        if (!curPtr[j - 1] && !curPtr[j] && !(curPtr[j + 1] & 0xfc)) {
                            j++;
                            offset_add ++;
                        }
                    }

                    pSlice->entry_point_offset[i] = size + offset_add;
                    ctb_row ++;
                }
            }
            bs[bs_main_id].Reset();
            PutSliceHeader(&bs[bs_main_id], pSlice);
            overheadBytes += H265Bs_GetBsSize(&bs[bs_main_id]);
            if (m_pps.entropy_coding_sync_enabled_flag) {
                for (Ipp32u row = pSlice->row_first; row <= pSlice->row_last; row++) {
                    Ipp32s size = H265Bs_GetBsSize(&bs[row]);
                    small_memcpy(bs[bs_main_id].m_base.m_pbs, bs[row].m_base.m_pbsBase, size);
                    bs[bs_main_id].m_base.m_pbs += size;
                }
            }
            nal.nal_unit_type = (Ipp8u)(pSlice->IdrPicFlag ? NAL_UT_CODED_SLICE_IDR :
                (m_pCurrentFrame->PicOrderCnt() >= 0 ? NAL_UT_CODED_SLICE_TRAIL_R :
                    (m_pCurrentFrame->m_isBRef ? NAL_UT_CODED_SLICE_DLP : NAL_UT_CODED_SLICE_RADL_N)));
            bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
        }
    }

    Ipp32s frameBytes = mfxBS->DataLength - initialDatalength;

    if (m_brc) {
        mfxBRCStatus brcSts;
        brcSts =  m_brc->PostPackFrame((mfxU16)m_pCurrentFrame->m_PicCodType, frameBytes << 3, overheadBytes << 3, brcRecode, m_pCurrentFrame->PicOrderCnt());
        brcRecode = 0;
        if (brcSts != MFX_BRC_OK) {
            if ((brcSts & MFX_BRC_ERR_SMALL_FRAME) && (m_videoParam.m_sliceQpY < min_qp))
                brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;
            if (!(brcSts & MFX_BRC_NOT_ENOUGH_BUFFER)) {
                mfxBS->DataLength = dataLength0;
                H265Bs_SetState(&bs[bs_main_id], pbs0, bitOffset0);
                overheadBytes = overheadBytes0;
                brcRecode = 1;
                goto recode;
            }
            else if (brcSts & MFX_BRC_ERR_SMALL_FRAME) {
                Ipp32s maxSize, minSize, bitsize = frameBytes << 3;
                Ipp8u *p = mfxBS->Data + mfxBS->DataOffset + mfxBS->DataLength;
                m_brc->GetMinMaxFrameSize(&minSize, &maxSize);
                if (minSize >  ((Ipp32s)mfxBS->MaxLength << 3))
                    return MFX_ERR_NOT_ENOUGH_BUFFER;
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
                m_brc->PostPackFrame((mfxU16)m_pCurrentFrame->m_PicCodType, bitsize, (overheadBytes << 3) + bitsize - (frameBytes << 3), 1, m_pCurrentFrame->PicOrderCnt());
                mfxBS->DataLength += (bitsize >> 3) - frameBytes;
            }
            else {
                return MFX_ERR_NOT_ENOUGH_BUFFER;
            }
        }
    }

    switch (ePic_Class)
    {
    case IDR_PIC:
    case REFERENCE_PIC:
        if (m_videoParam.enableCmFlag == 0)
            m_pCurrentFrame->SetisShortTermRef();
        m_pReconstructFrame->doPadding();
        break;

    case DISPOSABLE_PIC:
    default:
        break;
    }

    m_pCurrentFrame->swapData(m_pReconstructFrame);
    // for enableCmFlag m_dpb does not have current frame
    m_pCurrentFrame->Dump(m_recon_dump_file_name, &m_videoParam, &m_dpb, m_frameCountEncoded);

    if (m_videoParam.enableCmFlag == 0) {
        m_pCurrentFrame->setWasEncoded();
        CleanDPB();
    }

    m_frameCountEncoded++;

    return MFX_ERR_NONE;
}

#if defined (MFX_ENABLE_H265_PAQ)

mfxStatus H265Encoder::PreEncAnalysis(mfxBitstream* mfxBS)
{
    if (m_preEnc.m_stagesToGo == 0) {
        m_preEnc.m_stagesToGo = m_preEnc.m_emulatorForAsyncPart.Go( !m_cpb.isEmpty() );
    }
    if (m_preEnc.m_stagesToGo & AsyncRoutineEmulator::STG_BIT_ACCEPT_FRAME) {
        m_preEnc.m_stagesToGo &= ~AsyncRoutineEmulator::STG_BIT_ACCEPT_FRAME;
    }
    if (m_preEnc.m_stagesToGo & AsyncRoutineEmulator::STG_BIT_START_LA) {
        H265Frame* curFrameInEncodeOrder = m_cpb.findOldestToLookAheadProcess();
        //printf("\n curFrameInEncodeOrder = %p \n", curFrameInEncodeOrder);fflush(stderr);

        if(curFrameInEncodeOrder) {
            curFrameInEncodeOrder->setWasLAProcessed();
        }
        if(curFrameInEncodeOrder) {
                mfxFrameSurface1 inputSurface;
                inputSurface.Info.Width = curFrameInEncodeOrder->width;
                inputSurface.Info.Height = curFrameInEncodeOrder->height;
                inputSurface.Data.Pitch = curFrameInEncodeOrder->pitch_luma;
                inputSurface.Data.Y = curFrameInEncodeOrder->y;

                m_preEnc.m_inputSurface = &inputSurface;
                bool bStatus = m_preEnc.preAnalyzeOne(m_preEnc.m_acQPMap);
        } else {
            m_preEnc.m_inputSurface = NULL;
            bool bStatus = m_preEnc.preAnalyzeOne(m_preEnc.m_acQPMap); 
        }

        m_preEnc.m_stagesToGo &= ~AsyncRoutineEmulator::STG_BIT_START_LA;
    }

    if (m_preEnc.m_stagesToGo & AsyncRoutineEmulator::STG_BIT_WAIT_LA) {
        m_preEnc.m_stagesToGo &= ~AsyncRoutineEmulator::STG_BIT_WAIT_LA;
    }
    if (m_preEnc.m_stagesToGo & AsyncRoutineEmulator::STG_BIT_START_ENCODE) {
        m_preEnc.m_stagesToGo &= ~AsyncRoutineEmulator::STG_BIT_START_ENCODE;
    }
    if (m_preEnc.m_stagesToGo & AsyncRoutineEmulator::STG_BIT_WAIT_ENCODE) {
        m_preEnc.m_stagesToGo &= ~AsyncRoutineEmulator::STG_BIT_WAIT_ENCODE;
    }

    return MFX_ERR_NONE;

} // mfxStatus H265Encoder::PreEncAnalysis(mfxBitstream* mfxBS)
mfxStatus H265Encoder::UpdateAllLambda(void)
{
    int  origQP = m_videoParam.m_sliceQpY;
    for(int iDQpIdx = 0; iDQpIdx < 2*(MAX_DQP)+1; iDQpIdx++)  {
        int deltaQP = ((iDQpIdx+1)>>1)*(iDQpIdx%2 ? -1 : 1);
        int curQp = origQP + deltaQP;

        m_videoParam.m_dqpSlice[iDQpIdx].slice_type = m_slices[0].slice_type;
        m_videoParam.m_dqpSlice[iDQpIdx].pgop_idx = m_slices[0].pgop_idx;

        bool IsHiCplxGOP = false;
        bool IsMedCplxGOP = false;
        if ( 2 == m_videoParam.preEncMode ) {
            TQPMap *pcQPMapPOC = &m_preEnc.m_acQPMap[m_pCurrentFrame->PicOrderCnt() % m_preEnc.m_histLength];
            double SADpp = pcQPMapPOC->getAvgGopSADpp();
            double SCpp  = pcQPMapPOC->getAvgGopSCpp();
            if(SCpp>2.0) {
                double minSADpp = 0;
                if(m_videoParam.GopRefDist > 8) {
                    minSADpp = 1.3*SCpp - 2.6;
                    if(minSADpp>0 && minSADpp<SADpp) IsHiCplxGOP=true;
                    if(!IsHiCplxGOP) {
                        double minSADpp = 1.1*SCpp - 2.2;
                        if(minSADpp>0 && minSADpp<SADpp) IsMedCplxGOP=true;
                    }
                } else {
                    minSADpp = 1.1*SCpp - 1.5;
                    if(minSADpp>0 && minSADpp<SADpp) IsHiCplxGOP=true;
                    if(!IsHiCplxGOP) {
                        double minSADpp = 1.0*SCpp - 2.0;
                        if(minSADpp>0 && minSADpp<SADpp) IsMedCplxGOP=true;
                    }
                }
            }
        }

        SetAllLambda(&m_videoParam.m_dqpSlice[iDQpIdx], curQp, m_pCurrentFrame->PicOrderCnt(), IsHiCplxGOP, IsMedCplxGOP);
    }

    if ( m_videoParam.preEncMode > 1 ) {
        m_pAQP->m_POC = m_pCurrentFrame->PicOrderCnt();
        m_pAQP->m_SliceType = B_SLICE;
        if(MFX_FRAMETYPE_I == m_pCurrentFrame->m_PicCodType) {
            m_pAQP->m_SliceType = I_SLICE;
        }

        int iGOPid = m_pCurrentFrame->m_BpyramidNumFrame;
        m_pAQP->set_pic_coding_class(iGOPid);
        m_pAQP->setSliceQP( m_videoParam.m_sliceQpY );
    }

    return MFX_ERR_NONE;

} // mfxStatus H265Encoder::UpdateAllLambda(void)

template <typename PixType>
double compute_block_rscs(PixType *pSrcBuf, int width, int height, int stride)
{
    int        i, j;
    int        tmpVal, CsVal, RsVal;
    int        blkSizeC = (width-1) * height;
    int        blkSizeR = width * (height-1);
    double    RS, CS, RsCs;
    PixType    *pBuf;

    RS = CS = RsCs = 0.0;
    CsVal =    0;
    RsVal =    0;

    // CS
    CsVal =    0;
    pBuf = pSrcBuf;
    for(i = 0; i < height; i++) {
        for(j = 1; j < width; j++) {
            tmpVal = pBuf[j] - pBuf[j-1];
            CsVal += tmpVal * tmpVal;
        }
        pBuf += stride;
    }

    if(blkSizeC)
        CS = sqrt((double)CsVal / (double)blkSizeC);
    else {
        //fprintf(stderr, "ERROR: RSCS block %dx%d\n", width, height);
        VM_ASSERT(!"ERROR: RSCS block");
        CS = 0;
    }

    // RS
    pBuf = pSrcBuf;
    for(i = 1; i < height; i++) {
        for(j = 0; j < width; j++) {
            tmpVal = pBuf[j] - pBuf[j+stride];
            RsVal += tmpVal * tmpVal;
        }
        pBuf += stride;
    }

    if(blkSizeR)
        RS = sqrt((double)RsVal / (double)blkSizeR);
    else {
        VM_ASSERT(!"ERROR: RSCS block");
        RS = 0;
    }

    RsCs = sqrt(CS * CS + RS * RS);

    return RsCs;

} // double compute_block_rscs(PixType *pSrcBuf, int width, int height, int stride)

template <typename PixType>
int H265CU<PixType>::GetCalqDeltaQp(TAdapQP* sliceAQP, Ipp64f sliceLambda)
{
    //int poc = m_pCurrentFrame->PicOrderCnt();
    TAdapQP ctbAQP;

    ctbAQP.m_cuAddr = m_ctbAddr;
    ctbAQP.m_Xcu    = m_ctbPelX;
    ctbAQP.m_Ycu    = m_ctbPelY;

    if((ctbAQP.m_Xcu + sliceAQP->m_maxCUWidth) > sliceAQP->m_picWidth)  ctbAQP.m_cuWidth = sliceAQP->m_picWidth - ctbAQP.m_Xcu;
    else	ctbAQP.m_cuWidth = sliceAQP->m_maxCUWidth;

    if((ctbAQP.m_Ycu + sliceAQP->m_maxCUHeight) > sliceAQP->m_picHeight)  ctbAQP.m_cuHeight = sliceAQP->m_picHeight - ctbAQP.m_Ycu;
    else	ctbAQP.m_cuHeight = sliceAQP->m_maxCUHeight;

    ctbAQP.m_rscs = //ctbAQP.
        compute_block_rscs(
        m_ySrc,
        ctbAQP.m_cuWidth,
        ctbAQP.m_cuHeight,
        m_pitchSrc);

    ctbAQP.m_GOPSize = sliceAQP->m_GOPSize;
    ctbAQP.m_picClass = sliceAQP->m_picClass;
    ctbAQP.setSliceQP(sliceAQP->m_sliceQP);
    ctbAQP.setClass_RSCS(ctbAQP.m_rscs);

    int calq = (ctbAQP.getQPFromLambda(sliceLambda * 256.0) - sliceAQP->m_sliceQP);

    return calq;

} // int H265CU<PixType>::GetCalqDeltaQp(TAdapQP* sliceAQP, Ipp64f sliceLambda)
#else
    mfxStatus H265Encoder::PreEncAnalysis(mfxBitstream* mfxBS)
    {
        mfxBS; return MFX_ERR_NONE;

    }
    mfxStatus H265Encoder::UpdateAllLambda(void)
    {
        return MFX_ERR_NONE;
    }

    template <typename PixType>
    int H265CU<PixType>::GetCalqDeltaQp(TAdapQP* sliceAQP, Ipp64f sliceLambda)
    {
        sliceAQP;
        sliceLambda;

        return 0;
    }
#endif

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
