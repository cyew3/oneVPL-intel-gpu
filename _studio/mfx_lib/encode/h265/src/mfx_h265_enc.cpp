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
#include <xutility>

#include "ippi.h"
#include "vm_interlocked.h"
#include "vm_thread.h"
#include "vm_event.h"
#include "vm_sys_info.h"

#include "mfx_h265_encode.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_brc.h"

#include "mfx_h265_paq.h"

#if defined (MFX_VA)
#include "mfx_h265_enc_fei.h"
#endif // MFX_VA

#ifdef MFX_ENABLE_WATERMARK
#include "watermark.h"
#endif

namespace H265Enc {

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

//static 
Ipp64f h265_calc_split_threshold(Ipp32s tabIndex, Ipp32s isNotCu, Ipp32s isNotI, Ipp32s log2width, Ipp32s strength, Ipp32s QP)
{
    thres_tab *h265_split_thresholds_tab;

    switch(tabIndex) {
    case 1:
        h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_1 : h265_split_thresholds_cu_1;
        break;
    default:
    case 0:
    case 2:
        h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_4 : h265_split_thresholds_cu_4;
        break;
    case 3:
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

mfxStatus InitH265VideoParam(const mfxVideoParam *param /* IN */, H265VideoParam *pars /* OUT */, const mfxExtCodingOptionHEVC *optsHevc)
{
    //H265VideoParam *pars = &m_videoParam;
    Ipp32u width = param->mfx.FrameInfo.Width;
    Ipp32u height = param->mfx.FrameInfo.Height;

    if (param->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 || param->mfx.FrameInfo.FourCC == MFX_FOURCC_P210) {
        pars->bitDepthLuma = 10;
        pars->bitDepthChroma = 10;
    } else {
        pars->bitDepthLuma = 8;
        pars->bitDepthChroma = 8;
    }
    pars->bitDepthLumaShift = pars->bitDepthLuma - 8;
    pars->bitDepthChromaShift = pars->bitDepthChroma - 8;

    if (param->mfx.FrameInfo.FourCC == MFX_FOURCC_NV16 || param->mfx.FrameInfo.FourCC == MFX_FOURCC_P210) {
        pars->chromaFormatIdc = MFX_CHROMAFORMAT_YUV422;
    } else {
        pars->chromaFormatIdc = MFX_CHROMAFORMAT_YUV420;
    }
    pars->chroma422 = pars->chromaFormatIdc == MFX_CHROMAFORMAT_YUV422;
    pars->chromaShiftW = pars->chromaFormatIdc != MFX_CHROMAFORMAT_YUV444;
    pars->chromaShiftH = pars->chromaFormatIdc == MFX_CHROMAFORMAT_YUV420;
    pars->chromaShiftWInv = 1 - pars->chromaShiftW;
    pars->chromaShiftHInv = 1 - pars->chromaShiftH;
    pars->chromaShift = pars->chromaShiftW + pars->chromaShiftH;

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
    pars->BiPyramidLayers = (optsHevc->BPyramid == MFX_CODINGOPTION_ON) ? H265_CeilLog2(pars->GopRefDist) + 1 : 1;
    pars->longGop = (optsHevc->BPyramid == MFX_CODINGOPTION_ON && pars->GopRefDist == 16 && param->mfx.NumRefFrame == 5);

    pars->NumRefToStartCodeBSlice = 1;
    pars->TreatBAsReference = 0;
    pars->GeneralizedPB = (optsHevc->GPB == MFX_CODINGOPTION_ON);

    pars->MaxDecPicBuffering = MAX(param->mfx.NumRefFrame, pars->BiPyramidLayers);
    pars->MaxRefIdxP[0] = param->mfx.NumRefFrame;
    pars->MaxRefIdxP[1] = pars->GeneralizedPB ? param->mfx.NumRefFrame : 0;
    pars->MaxRefIdxB[0] = 0;
    pars->MaxRefIdxB[1] = 0;

    if (pars->GopRefDist > 1) {
        if (pars->longGop) {
            pars->MaxRefIdxB[0] = 2;
            pars->MaxRefIdxB[1] = 2;
        }
        else if (pars->BiPyramidLayers > 1) {
            pars->MaxRefIdxB[0] = (param->mfx.NumRefFrame + 1) / 2;
            pars->MaxRefIdxB[1] = IPP_MAX(1, (param->mfx.NumRefFrame + 0) / 2);
        }
        else {
            pars->MaxRefIdxB[0] = param->mfx.NumRefFrame - 1;
            pars->MaxRefIdxB[1] = 1;
        }
    }

    pars->PGopPicSize = (pars->GopRefDist == 1 && pars->MaxRefIdxP[0] > 1) ? PGOP_PIC_SIZE : 1;

    pars->AnalyseFlags = 0;
    if (optsHevc->AnalyzeChroma == MFX_CODINGOPTION_ON)
        pars->AnalyseFlags |= HEVC_ANALYSE_CHROMA;
    if (optsHevc->CostChroma == MFX_CODINGOPTION_ON)
        pars->AnalyseFlags |= HEVC_COST_CHROMA;

    pars->SplitThresholdStrengthCUIntra = (Ipp8u)optsHevc->SplitThresholdStrengthCUIntra - 1;
    pars->SplitThresholdStrengthTUIntra = (Ipp8u)optsHevc->SplitThresholdStrengthTUIntra - 1;
    pars->SplitThresholdStrengthCUInter = (Ipp8u)optsHevc->SplitThresholdStrengthCUInter - 1;
    pars->SplitThresholdTabIndex        = (Ipp8u)optsHevc->SplitThresholdTabIndex;

    pars->FastInterp  = (optsHevc->FastInterp == MFX_CODINGOPTION_ON);
    pars->cpuFeature = optsHevc->CpuFeature;
    pars->IntraChromaRDO  = (optsHevc->IntraChromaRDO == MFX_CODINGOPTION_ON);
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
        pars->num_cand_0[0][i] = 33;
        pars->num_cand_0[1][i] = 33;
        pars->num_cand_0[2][i] = 33;
        pars->num_cand_0[3][i] = 33;
        pars->num_cand_1[i] = w > 8 ? 4 : 8;
        pars->num_cand_2[i] = pars->num_cand_1[i] >> 1;
    }
    //making a copy for future if we want to extend control for every frame type
    //so far just a copy
    if (optsHevc->IntraNumCand0_2) { 
        pars->num_cand_0[0][2] = (Ipp8u)optsHevc->IntraNumCand0_2;pars->num_cand_0[1][2] = (Ipp8u)optsHevc->IntraNumCand0_2;
        pars->num_cand_0[2][2] = (Ipp8u)optsHevc->IntraNumCand0_2;pars->num_cand_0[3][2] = (Ipp8u)optsHevc->IntraNumCand0_2;
    }
    if (optsHevc->IntraNumCand0_3) { 
        pars->num_cand_0[0][3] = (Ipp8u)optsHevc->IntraNumCand0_3; pars->num_cand_0[1][3] = (Ipp8u)optsHevc->IntraNumCand0_3;
        pars->num_cand_0[2][3] = (Ipp8u)optsHevc->IntraNumCand0_3; pars->num_cand_0[3][3] = (Ipp8u)optsHevc->IntraNumCand0_3;
    }
    if (optsHevc->IntraNumCand0_4) { 
        pars->num_cand_0[0][4] = (Ipp8u)optsHevc->IntraNumCand0_4; pars->num_cand_0[1][4] = (Ipp8u)optsHevc->IntraNumCand0_4;
        pars->num_cand_0[2][4] = (Ipp8u)optsHevc->IntraNumCand0_4; pars->num_cand_0[3][4] = (Ipp8u)optsHevc->IntraNumCand0_4;
    }
    if (optsHevc->IntraNumCand0_5) { 
        pars->num_cand_0[0][5] = (Ipp8u)optsHevc->IntraNumCand0_5; pars->num_cand_0[1][5] = (Ipp8u)optsHevc->IntraNumCand0_5;
        pars->num_cand_0[2][5] = (Ipp8u)optsHevc->IntraNumCand0_5; pars->num_cand_0[3][5] = (Ipp8u)optsHevc->IntraNumCand0_5;
    }
    if (optsHevc->IntraNumCand0_6) {
        pars->num_cand_0[0][6] = (Ipp8u)optsHevc->IntraNumCand0_6; pars->num_cand_0[1][6] = (Ipp8u)optsHevc->IntraNumCand0_6;
        pars->num_cand_0[2][6] = (Ipp8u)optsHevc->IntraNumCand0_6; pars->num_cand_0[3][6] = (Ipp8u)optsHevc->IntraNumCand0_6;
    }
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

#if defined (MFX_VA)
    pars->enableCmFlag = (optsHevc->EnableCm == MFX_CODINGOPTION_ON);
#else
    pars->enableCmFlag = 0;
#endif // MFX_VA

    pars->preEncMode = 0;
#if defined(MFX_ENABLE_H265_PAQ)
    // support for 64x64 && BPyramid && RefDist==8 || 16 (aka tu1 & tu2)
    //if( pars->BPyramid && (8 == pars->GopRefDist || 16 == pars->GopRefDist) && (6 == pars->Log2MaxCUSize) )
        pars->preEncMode = optsHevc->DeltaQpMode;
#endif
    pars->TryIntra         = optsHevc->TryIntra;
    pars->FastAMPSkipME    = optsHevc->FastAMPSkipME;
    pars->FastAMPRD    = optsHevc->FastAMPRD;
    pars->SkipMotionPartition    = optsHevc->SkipMotionPartition;
    pars->cmIntraThreshold = optsHevc->CmIntraThreshold;
    pars->tuSplitIntra = optsHevc->TUSplitIntra;
    pars->cuSplit = optsHevc->CUSplit;
    pars->intraAngModes[0] = optsHevc->IntraAngModes;
    pars->intraAngModes[1] = optsHevc->IntraAngModesP;
    pars->intraAngModes[2] = optsHevc->IntraAngModesBRef;
    pars->intraAngModes[3] = optsHevc->IntraAngModesBnonRef;
    pars->fastSkip = (optsHevc->FastSkip == MFX_CODINGOPTION_ON);
    pars->SkipCandRD = (optsHevc->SkipCandRD == MFX_CODINGOPTION_ON);
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
    pars->maxCUDepthAdapt = (optsHevc->MaxCUDepthAdapt == MFX_CODINGOPTION_ON);
    pars->cuSplitThreshold = optsHevc->CUSplitThreshold;

    for (Ipp32s i = 0; i <= 6; i++) {
            //to make this work we need extend input user's NumCand0 for every frame type
            //so far all 4 just a copy        
        if (INTRA_ANG_MODE_GRADIENT == pars->intraAngModes[0]) { 
            pars->num_cand_0[0][i] = Saturate(1, 33, pars->num_cand_0[0][i]);
            pars->num_cand_1[i] = Saturate(1, pars->num_cand_0[0][i] + 2, pars->num_cand_1[i]);//AFfix - own ang mode for every slice type now
        }
        else {
            pars->num_cand_1[i] = Saturate(1, 35, pars->num_cand_1[i]);
        }
        if (INTRA_ANG_MODE_GRADIENT == pars->intraAngModes[1]) { 
            pars->num_cand_0[1][i] = Saturate(1, 33, pars->num_cand_0[1][i]);
        }
        if (INTRA_ANG_MODE_GRADIENT == pars->intraAngModes[2]) { 
            pars->num_cand_0[2][i] = Saturate(1, 33, pars->num_cand_0[2][i]);
        }
        if (INTRA_ANG_MODE_GRADIENT == pars->intraAngModes[3]) { 
            pars->num_cand_0[3][i] = Saturate(1, 33, pars->num_cand_0[3][i]);
        }
        pars->num_cand_2[i] = Saturate(1, pars->num_cand_1[i], pars->num_cand_2[i]);
    }

    /* derived */

    pars->MaxTrSize = 1 << pars->QuadtreeTULog2MaxSize;
    pars->MaxCUSize = 1 << pars->Log2MaxCUSize;

    pars->AddCUDepth  = 0;
    while ((pars->MaxCUSize>>pars->MaxCUDepth) >
        (1u << ( pars->QuadtreeTULog2MinSize + pars->AddCUDepth))) {
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

    /*if (pars->num_threads > pars->PicHeightInCtbs)
        pars->num_threads = pars->PicHeightInCtbs;
    if (pars->num_threads > (pars->PicWidthInCtbs + 1) / 2)
        pars->num_threads = (pars->PicWidthInCtbs + 1) / 2;*/

    //pars->threading_by_rows = pars->WPPFlag && pars->num_threads > 1 && pars->NumSlices == 1 ? 0 : 1;
    //pars->num_thread_structs = pars->threading_by_rows ? pars->num_threads : pars->PicHeightInCtbs;
//pars->threading_by_rows = pars->WPPFlag && pars->num_threads > 1 && pars->NumSlices == 1 ? 0 : 1;
    pars->threading_by_rows = 0;//pars->WPPFlag && /*pars->num_threads > 1 &&*/ pars->NumSlices == 1 ? 0 : 1;
    pars->num_thread_structs = pars->WPPFlag ? pars->PicHeightInCtbs : pars->num_threads;

    for (Ipp32s i = 0; i < pars->MaxCUDepth; i++ )
        pars->AMPAcc[i] = i < pars->MaxCUDepth-pars->AddCUDepth ? (pars->partModes==3) : 0;

    // deltaQP control, main control params
    pars->MaxCuDQPDepth = 0;
    pars->m_maxDeltaQP = 0;
    pars->UseDQP = 0;
    
    if (pars->preEncMode) {
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

    pars->tcDuration90KHz = (mfxF64)param->mfx.FrameInfo.FrameRateExtD / param->mfx.FrameInfo.FrameRateExtN * 90000; // calculate tick duration    
    pars->m_framesInParallel = optsHevc->FramesInParallel;    

    return MFX_ERR_NONE;
}
}


mfxStatus MFXVideoENCODEH265::SetVPS()
{
    H265VidParameterSet *vps = &m_vps;

    memset(vps, 0, sizeof(H265VidParameterSet));
    vps->vps_max_layers = 1;
    vps->vps_max_sub_layers = 1;
    vps->vps_temporal_id_nesting_flag = 1;
    
    vps->vps_max_dec_pic_buffering[0] = m_videoParam.MaxDecPicBuffering;
    vps->vps_max_num_reorder_pics[0] = 0;
    if (m_videoParam.GopRefDist > 1)
        vps->vps_max_num_reorder_pics[0] = MAX(1, m_videoParam.BiPyramidLayers - 1);

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

mfxStatus MFXVideoENCODEH265::SetProfileLevel()
{
    memset(&m_profile_level, 0, sizeof(H265ProfileLevelSet));
    m_profile_level.general_profile_idc = (mfxU8) m_videoParam.Profile; //MFX_PROFILE_HEVC_MAIN;
    m_profile_level.general_tier_flag = (mfxU8) m_videoParam.Tier;
    m_profile_level.general_level_idc = (mfxU8) (m_videoParam.Level * 3);
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::SetSPS()
{
    H265SeqParameterSet *sps = &m_sps;
    H265VidParameterSet *vps = &m_vps;
    H265VideoParam *pars = &m_videoParam;

    memset(sps, 0, sizeof(H265SeqParameterSet));

    sps->sps_video_parameter_set_id = vps->vps_video_parameter_set_id;
    sps->sps_max_sub_layers = 1;
    sps->sps_temporal_id_nesting_flag = 1;

    sps->chroma_format_idc = pars->chromaFormatIdc;

    sps->pic_width_in_luma_samples = pars->Width;
    sps->pic_height_in_luma_samples = pars->Height;
    sps->bit_depth_luma = pars->bitDepthLuma;
    sps->bit_depth_chroma = pars->bitDepthChroma;

    if (pars->CropLeft | pars->CropTop | pars->CropRight | pars->CropBottom) {
        sps->conformance_window_flag = 1;
        sps->conf_win_left_offset = pars->CropLeft >> m_videoParam.chromaShiftW;
        sps->conf_win_top_offset = pars->CropTop >> m_videoParam.chromaShiftH;
        sps->conf_win_right_offset = pars->CropRight >> m_videoParam.chromaShiftW;
        sps->conf_win_bottom_offset = pars->CropBottom >> m_videoParam.chromaShiftH;
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

    if (m_videoParam.GopRefDist == 1) {
        sps->log2_max_pic_order_cnt_lsb = 4;
    } else {
        Ipp32s log2_max_poc = H265_CeilLog2(m_videoParam.GopRefDist - 1 + m_videoParam.MaxDecPicBuffering) + 3;

        sps->log2_max_pic_order_cnt_lsb = (Ipp8u)MAX(log2_max_poc, 4);

        if (sps->log2_max_pic_order_cnt_lsb > 16) {
            VM_ASSERT(false);
            sps->log2_max_pic_order_cnt_lsb = 16;
        }
    }

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

mfxStatus MFXVideoENCODEH265::SetPPS()
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
    pps->pps_loop_filter_across_slices_enabled_flag = 0; // npshosta: otherwise can't do deblocking in the same thread
    pps->loop_filter_across_tiles_enabled_flag = 1;
    pps->entropy_coding_sync_enabled_flag = m_videoParam.WPPFlag;

    if (m_brc)
        pps->init_qp = (Ipp8s)m_brc->GetQP(m_videoParam, NULL);
    else
        pps->init_qp = m_videoParam.QPI;

    pps->log2_parallel_merge_level = 2;

    if (m_videoParam.GopRefDist == 1) { // most frames are P frames
        pps->num_ref_idx_l0_default_active = IPP_MAX(1, m_videoParam.MaxRefIdxP[0]);
        pps->num_ref_idx_l1_default_active = IPP_MAX(1, m_videoParam.MaxRefIdxP[1]);
    }
    else { // most frames are B frames
        pps->num_ref_idx_l0_default_active = IPP_MAX(1, m_videoParam.MaxRefIdxB[0]);
        pps->num_ref_idx_l1_default_active = IPP_MAX(1, m_videoParam.MaxRefIdxB[1]);
    }

    pps->sign_data_hiding_enabled_flag = m_videoParam.SBHFlag;
    
    if (m_videoParam.UseDQP) {
        pps->cu_qp_delta_enabled_flag = 1;
        pps->diff_cu_qp_delta_depth   = m_videoParam.MaxCuDQPDepth;
    }
    return MFX_ERR_NONE;
}

const Ipp32f tab_rdLambdaBPyramid5LongGop[5]  = {0.442f, 0.3536f, 0.3536f, 0.4000f, 0.68f};
const Ipp32f tab_rdLambdaBPyramid5[5]         = {0.442f, 0.3536f, 0.3536f, 0.3536f, 0.68f};
const Ipp32f tab_rdLambdaBPyramid4[4]         = {0.442f, 0.3536f, 0.3536f, 0.68f};
const Ipp32f tab_rdLambdaBPyramid_LoCmplx[4]  = {0.442f, 0.3536f, 0.4000f, 0.68f};
const Ipp32f tab_rdLambdaBPyramid_MidCmplx[4] = {0.442f, 0.3536f, 0.3817f, 0.60f};
const Ipp32f tab_rdLambdaBPyramid_HiCmplx[4]  = {0.442f, 0.2793f, 0.3536f, 0.50f};


mfxStatus MFXVideoENCODEH265::SetSlice(H265Slice *slice, Ipp32u curr_slice, H265Frame *frame)
{
    memset(slice, 0, sizeof(H265Slice));

    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs * m_videoParam.PicHeightInCtbs;
    Ipp32u numSlices = m_videoParam.NumSlices;
    Ipp32u slice_len = numCtbs / numSlices;

    // aya: quick fix for support multi-slice mode 
    if ( numSlices > 1 ) {
        slice_len = slice_len - (slice_len % m_videoParam.PicWidthInCtbs);
    }
    slice->slice_segment_address = slice_len * curr_slice;
    slice->slice_address_last_ctb = slice_len * (curr_slice + 1) - 1;
    if (slice->slice_address_last_ctb > numCtbs - 1 || curr_slice == numSlices - 1)
        slice->slice_address_last_ctb = numCtbs - 1;

    if (curr_slice) slice->first_slice_segment_in_pic_flag = 0;
    else slice->first_slice_segment_in_pic_flag = 1;

    slice->slice_pic_parameter_set_id = m_pps.pps_pic_parameter_set_id;

    switch (frame->m_picCodeType) {
    case MFX_FRAMETYPE_P:
        slice->slice_type = (m_videoParam.GeneralizedPB) ? B_SLICE : P_SLICE; break;
    case MFX_FRAMETYPE_B:
        slice->slice_type = B_SLICE; break;
    case MFX_FRAMETYPE_I:
    default:
        slice->slice_type = I_SLICE; break;
    }

    slice->sliceIntraAngMode = EnumIntraAngMode((frame->m_picCodeType == MFX_FRAMETYPE_B && !frame->m_isRef)
        ? m_videoParam.intraAngModes[B_NONREF]
        : m_videoParam.intraAngModes[SliceTypeIndex(slice->slice_type)]);

    if (frame->m_isIdrPic) {
        slice->IdrPicFlag = 1;
        slice->RapPicFlag = 1;
    }

    slice->slice_pic_order_cnt_lsb = frame->m_poc & ~(0xffffffff << m_sps.log2_max_pic_order_cnt_lsb);
    slice->deblocking_filter_override_flag = 0;
    slice->slice_deblocking_filter_disabled_flag = m_pps.pps_deblocking_filter_disabled_flag;
    slice->slice_tc_offset_div2 = m_pps.pps_tc_offset_div2;
    slice->slice_beta_offset_div2 = m_pps.pps_beta_offset_div2;
    slice->slice_loop_filter_across_slices_enabled_flag = m_pps.pps_loop_filter_across_slices_enabled_flag;
    slice->slice_cb_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_cr_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_temporal_mvp_enabled_flag = m_sps.sps_temporal_mvp_enabled_flag;

    slice->num_ref_idx[0] = frame->m_refPicList[0].m_refFramesCount;
    slice->num_ref_idx[1] = frame->m_refPicList[1].m_refFramesCount;

    if (slice->slice_type == P_SLICE)
        slice->num_ref_idx_active_override_flag =
            (slice->num_ref_idx[0] != m_pps.num_ref_idx_l0_default_active);
    else if (slice->slice_type == B_SLICE)
        slice->num_ref_idx_active_override_flag =
            (slice->num_ref_idx[0] != m_pps.num_ref_idx_l0_default_active) ||
            (slice->num_ref_idx[1] != m_pps.num_ref_idx_l1_default_active);

    slice->short_term_ref_pic_set_sps_flag = 1;
    slice->short_term_ref_pic_set_idx = frame->m_RPSIndex;

    slice->slice_num = curr_slice;

    slice->five_minus_max_num_merge_cand = 5 - MAX_NUM_MERGE_CANDS;

    if (m_pps.entropy_coding_sync_enabled_flag) {
        slice->row_first = slice->slice_segment_address / m_videoParam.PicWidthInCtbs;
        slice->row_last = slice->slice_address_last_ctb / m_videoParam.PicWidthInCtbs;
        slice->num_entry_point_offsets = slice->row_last - slice->row_first;
    }

    return MFX_ERR_NONE;
} 

namespace H265Enc {

void SetAllLambda(H265VideoParam const & videoParam, H265Slice *slice, Ipp32s qp, const H265Frame *currFrame, bool isHiCmplxGop, bool isMidCmplxGop)
{
    {
    slice->rd_opt_flag = 1;
    slice->rd_lambda_slice = 1;
    if (slice->rd_opt_flag) {
        slice->rd_lambda_slice = pow(2.0, (qp - 12) * (1.0 / 3.0)) * (1.0 / 256.0);
        switch (slice->slice_type) {
        case P_SLICE:
            if (videoParam.BiPyramidLayers > 1 && OPT_LAMBDA_PYRAMID) {
                slice->rd_lambda_slice *= (currFrame->m_biFramesInMiniGop == 15)
                    ? (videoParam.longGop)
                        ? tab_rdLambdaBPyramid5LongGop[0]
                        : tab_rdLambdaBPyramid5[0]
                    : tab_rdLambdaBPyramid4[0];
            }
            else {
                Ipp32s pgopIndex = (currFrame->m_frameOrder - currFrame->m_frameOrderOfLastIntra) % videoParam.PGopPicSize;
                slice->rd_lambda_slice *= (videoParam.PGopPicSize == 1 || pgopIndex) ? 0.4624 : 0.578;
                if (pgopIndex)
                    slice->rd_lambda_slice *= Saturate(2, 4, (qp - 12) / 6.0);
            }
            break;
        case B_SLICE:
            if (videoParam.BiPyramidLayers > 1 && OPT_LAMBDA_PYRAMID) {
                Ipp8u layer = currFrame->m_pyramidLayer;
                if (videoParam.preEncMode > 1) {
                    if (isHiCmplxGop)
                        slice->rd_lambda_slice *= tab_rdLambdaBPyramid_HiCmplx[layer];
                    else if (isMidCmplxGop)
                        slice->rd_lambda_slice *= tab_rdLambdaBPyramid_MidCmplx[layer];
                    else
                        slice->rd_lambda_slice *= tab_rdLambdaBPyramid_LoCmplx[layer];
                }
                else {
                    slice->rd_lambda_slice *= (currFrame->m_biFramesInMiniGop == 15)
                        ? (videoParam.longGop)
                            ? tab_rdLambdaBPyramid5LongGop[layer]
                            : tab_rdLambdaBPyramid5[layer]
                        : tab_rdLambdaBPyramid4[layer];
                }
                if (layer > 0)
                    slice->rd_lambda_slice *= Saturate(2, 4, (qp - 12) / 6.0);
            }
            else {
                slice->rd_lambda_slice *= 0.4624;
                slice->rd_lambda_slice *= Saturate(2, 4, (qp - 12) / 6.0);
            }
            break;
        case I_SLICE:
        default:
            slice->rd_lambda_slice *= 0.57;
            if (videoParam.GopRefDist > 1)
                slice->rd_lambda_slice *= (1 - MIN(0.5, 0.05 * (videoParam.GopRefDist - 1)));
        }
    }

    slice->rd_lambda_inter_slice = slice->rd_lambda_slice;
    slice->rd_lambda_inter_mv_slice = slice->rd_lambda_inter_slice;

    slice->rd_lambda_sqrt_slice = sqrt(slice->rd_lambda_slice * 256);
    //no chroma QP offset (from PPS) is implemented yet
    slice->ChromaDistWeight_slice = pow(2.0, (qp - h265_QPtoChromaQP[videoParam.chromaFormatIdc - 1][qp]) / 3.0);
}

} //
}

static Ipp8u dpoc_neg[][16][6] = {
    //GopRefDist 4
    {{2, 4, 2},
    {1, 1},
    {1, 2},
    {1, 1}},

    //GopRefDist 8
    {{4, 8, 2, 2, 4}, //poc 8
    {1, 1},           //poc 1
    {2, 2, 2},
    {2, 1, 2},
    {3, 4, 2, 2},
    {2, 1, 4},
    {3, 2, 2, 2},
    {3, 1, 2, 4}},

    //GopRefDist 16. 
    // it isn't BPyramid. it is "anchor & chain"
    {{5, 16, 2, 2, 4, 8}, //poc 16
    {2, 1, 16},           // poc 1
    {3, 2, 4, 12},
    {3, 1, 2, 16},
    {4, 4, 4, 4, 8},
    {2, 1, 4},
    {3, 2, 2, 2},
    {3, 1, 2, 4},
    {4, 4, 2, 2, 16},
    {2, 1, 8},
    {3, 2, 2, 6},
    {3, 1, 2, 8},
    {4, 4, 2, 2, 4},
    {3, 1, 4, 8},
    {4, 2, 2, 2, 8},
    {4, 1, 2, 4, 8}}  // poc 15
};

static Ipp8u dpoc_pos[][16][6] = {
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
    {1, 1}},

    //GopRefDist 16
    {{0,},        // poc 16 
    {3, 1, 2, 12},// poc 1
    {2, 2, 12},
    {2, 1, 12},
    {1, 12},
    {3, 1, 2, 8},
    {2, 2, 8},
    {2, 1, 8},
    {1, 8},
    {3, 1, 2, 4},
    {2, 2, 4},
    {2, 1, 4},
    {1, 4},
    {2, 1, 2},
    {1, 2},
    {1, 1}} // poc 15
};

void MFXVideoENCODEH265::InitShortTermRefPicSet()
{
    Ipp32s gopRefDist = m_videoParam.GopRefDist;
    Ipp32s *maxRefP = m_videoParam.MaxRefIdxP;
    Ipp32s *maxRefB = m_videoParam.MaxRefIdxB;

    Ipp32u numShortTermRefPicSets = 0;

    if (m_videoParam.BiPyramidLayers > 1 && gopRefDist > 1) {
        if (!(gopRefDist ==  4 && maxRefP[0] == 2 && maxRefB[0] == 1 && maxRefB[1] == 1) &&
            !(gopRefDist ==  8 && maxRefP[0] == 4 && maxRefB[0] == 2 && maxRefB[1] == 2) &&
            !(gopRefDist == 16 && maxRefP[0] == 5 && maxRefB[0] == 2 && maxRefB[1] == 2))
        {
            // no rps in sps for non-standard pyramids
            m_sps.num_short_term_ref_pic_sets = 0;
            return;
        }

        Ipp8u r = (gopRefDist == 4) ? 0 : (gopRefDist == 8) ? 1 : 2;
        for (Ipp32s i = 0; i < gopRefDist; i++) {
            m_sps.m_shortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
            m_sps.m_shortRefPicSet[i].num_negative_pics = dpoc_neg[r][i][0];
            m_sps.m_shortRefPicSet[i].num_positive_pics = dpoc_pos[r][i][0];

            for (Ipp32s j = 0; j < m_sps.m_shortRefPicSet[i].num_negative_pics; j++) {
                m_sps.m_shortRefPicSet[i].delta_poc[j] = dpoc_neg[r][i][1 + j];
                m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
            }
            for (Ipp32s j = 0; j < m_sps.m_shortRefPicSet[i].num_positive_pics; j++) {
                m_sps.m_shortRefPicSet[i].delta_poc[m_sps.m_shortRefPicSet[i].num_negative_pics + j] = dpoc_pos[r][i][1 + j];
                m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[m_sps.m_shortRefPicSet[i].num_negative_pics + j] = 1;
            }
            numShortTermRefPicSets++;
        }
    }
    else {
        Ipp32s deltaPoc = m_videoParam.TreatBAsReference ? 1 : gopRefDist;

        for (Ipp32s i = 0; i < m_videoParam.PGopPicSize; i++) {
            Ipp32s idx = i == 0 ? 0 : gopRefDist - 1 + i;
            m_sps.m_shortRefPicSet[idx].inter_ref_pic_set_prediction_flag = 0;
            m_sps.m_shortRefPicSet[idx].num_negative_pics = maxRefP[0];
            m_sps.m_shortRefPicSet[idx].num_positive_pics = 0;
            for (Ipp32s j = 0; j < m_sps.m_shortRefPicSet[idx].num_negative_pics; j++) {
                Ipp32s deltaPoc_cur;
                if (j == 0)
                    deltaPoc_cur = 1;
                else if (j == 1)
                    deltaPoc_cur = (m_videoParam.PGopPicSize - 1 + i) % m_videoParam.PGopPicSize;
                else
                    deltaPoc_cur = m_videoParam.PGopPicSize;
                if (deltaPoc_cur == 0)
                    deltaPoc_cur =  m_videoParam.PGopPicSize;
                m_sps.m_shortRefPicSet[idx].delta_poc[j] = deltaPoc_cur * gopRefDist;
                m_sps.m_shortRefPicSet[idx].used_by_curr_pic_flag[j] = 1;
            }
            numShortTermRefPicSets++;
        }

        for (Ipp32s i = 1; i < gopRefDist; i++) {
            m_sps.m_shortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
            m_sps.m_shortRefPicSet[i].num_negative_pics = maxRefB[0];
            m_sps.m_shortRefPicSet[i].num_positive_pics = maxRefB[1];

            m_sps.m_shortRefPicSet[i].delta_poc[0] = m_videoParam.TreatBAsReference ? 1 : i;
            m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[0] = 1;
            for (Ipp32s j = 1; j < m_sps.m_shortRefPicSet[i].num_negative_pics; j++) {
                m_sps.m_shortRefPicSet[i].delta_poc[j] = deltaPoc;
                m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
            }
            for (Ipp32s j = 0; j < m_sps.m_shortRefPicSet[i].num_positive_pics; j++) {
                m_sps.m_shortRefPicSet[i].delta_poc[m_sps.m_shortRefPicSet[i].num_negative_pics + j] = gopRefDist - i;
                m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[m_sps.m_shortRefPicSet[i].num_negative_pics + j] = 1;
            }
            numShortTermRefPicSets++;
        }
    }

    m_sps.num_short_term_ref_pic_sets = (Ipp8u)numShortTermRefPicSets;
}

#if defined(DUMP_COSTS_CU) || defined (DUMP_COSTS_TU)
extern FILE *fp_cu, *fp_tu;
#endif


DispatchSaoApplyFilter::DispatchSaoApplyFilter()
{
    m_bitDepth = 0;//unknown
    m_sao     = NULL;
    m_sao10bit = NULL;
}

DispatchSaoApplyFilter::~DispatchSaoApplyFilter()
{
    Close();
}

mfxStatus DispatchSaoApplyFilter::Init(int width, int height, int maxCUWidth, int maxDepth, int bitDepth)
{
    if (m_bitDepth > 0) { //was inited!!!
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_bitDepth = bitDepth;
    if (m_bitDepth == 8) {
        m_sao = new SaoDecodeFilter<Ipp8u>;
        MFX_CHECK_NULL_PTR1(m_sao);
        m_sao->Init(width, height, maxCUWidth, maxDepth, bitDepth);

    } else if (m_bitDepth == 10) {
        m_sao10bit = new SaoDecodeFilter<Ipp16u>;
        MFX_CHECK_NULL_PTR1(m_sao10bit);
        m_sao10bit->Init(width, height, maxCUWidth, maxDepth, bitDepth);

    } else {
        m_bitDepth = 0;
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

void DispatchSaoApplyFilter::Close()
{
    if (m_bitDepth == 8) {
        delete m_sao;
        m_sao = NULL;
    } else if(m_bitDepth == 10) {
        delete m_sao10bit;
        m_sao10bit = NULL;
    }

    m_bitDepth = 0;//unknown
}


H265FrameEncoder::H265FrameEncoder() 
{
    memBuf = NULL; m_bs = NULL; m_bsf = NULL;
    data_temp = NULL;
    //m_slices = NULL;
    m_slice_ids = NULL;
    //m_pCurrentFrame = 
    m_pNextFrame = NULL;
    m_context_array_wpp = NULL;

#ifdef MFX_ENABLE_WATERMARK
    m_watermark = NULL;
#endif

} // H265FrameEncoder::H265FrameEncoder() 


mfxStatus H265FrameEncoder::Init(const mfxVideoParam* mfxParam, const H265VideoParam & videoParam, const mfxExtCodingOptionHEVC *optsHevc, VideoCORE *core)
{
#ifdef MFX_ENABLE_WATERMARK
    m_watermark = Watermark::CreateFromResource();
    if (NULL == m_watermark)
        return MFX_ERR_UNKNOWN;
#endif

    // !!!copy global params!!!
    m_videoParam = videoParam;

    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    Ipp32s sizeofH265CU = (m_videoParam.bitDepthLuma > 8 ? sizeof(H265CU<Ipp16u>) : sizeof(H265CU<Ipp8u>));
    
    data_temp_size = ((MAX_TOTAL_DEPTH * 2 + 2) << m_videoParam.Log2NumPartInCU);

    // temp buf size - todo reduce

    // [1] calc mem size to allocate
    Ipp32u streamBufSizeMain = m_videoParam.SourceWidth * m_videoParam.SourceHeight * 6 / (2 + m_videoParam.chromaShift) + DATA_ALIGN;
    Ipp32u streamBufSize = (m_videoParam.SourceWidth * (m_videoParam.WPPFlag ? m_videoParam.MaxCUSize : m_videoParam.SourceHeight)) * 6 / (2 + m_videoParam.chromaShift) + DATA_ALIGN;
    Ipp32u memSize = 0;
    
    // m_bs
    memSize += sizeof(H265BsReal)*(m_videoParam.num_thread_structs + 1) + DATA_ALIGN; 
    // m_bsf
    memSize += sizeof(H265BsFake)*m_videoParam.num_thread_structs + DATA_ALIGN;
    memSize += streamBufSize*m_videoParam.num_thread_structs + streamBufSizeMain; // data of m_bs* and m_bsFake*
    memSize += sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT * m_videoParam.PicHeightInCtbs; // cabac

    memSize += sizeof(H265CUData) * data_temp_size * m_videoParam.num_thread_structs + DATA_ALIGN;//for ModeDecision try different cu 

    // m_slice_id
    memSize += numCtbs; 

    // m_row_info
    memSize += sizeof(H265EncoderRowInfo) * m_videoParam.PicHeightInCtbs + DATA_ALIGN;
    memSize += sizeofH265CU * m_videoParam.num_thread_structs + DATA_ALIGN;//cu

    // m_costStat
    memSize += numCtbs * sizeof(costStat) + DATA_ALIGN;
    // end ------------------------------------------------

    // [2] allocation
    memBuf = (Ipp8u *)H265_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(memBuf);

    // [3] memory setting/mapping
    Ipp8u *ptr = memBuf;

    // m_bs
    m_bs = UMC::align_pointer<H265BsReal*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265BsReal)*(m_videoParam.num_thread_structs + 1) + DATA_ALIGN;
    // m_bsf
    m_bsf = UMC::align_pointer<H265BsFake*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265BsFake)*(m_videoParam.num_thread_structs) + DATA_ALIGN;

    // m_bs data
    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs+1; i++) {
        m_bs[i].m_base.m_pbsBase = ptr;
        m_bs[i].m_base.m_maxBsSize = streamBufSize;
        m_bs[i].Reset();
        ptr += streamBufSize;
    }
    m_bs[m_videoParam.num_thread_structs].m_base.m_maxBsSize = streamBufSizeMain;
    ptr += streamBufSizeMain - streamBufSize;

    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs; i++) {
        m_bsf[i].Reset();
    }

    // m_context_array_wpp
    m_context_array_wpp = ptr;
    ptr += sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT * m_videoParam.PicHeightInCtbs;

    // data_temp
    data_temp = UMC::align_pointer<H265CUData*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265CUData) * data_temp_size * m_videoParam.num_thread_structs + DATA_ALIGN;
    
    // m_slice_id
    m_slice_ids = ptr;
    ptr += numCtbs;

    // m_row_info
    //m_row_info.resize(m_videoParam.PicHeightInCtbs);
    m_row_info = UMC::align_pointer<H265EncoderRowInfo*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265EncoderRowInfo) * m_videoParam.PicHeightInCtbs + DATA_ALIGN;

    // cu
    cu = UMC::align_pointer<void*>(ptr, DATA_ALIGN);
    ptr += sizeofH265CU * m_videoParam.num_thread_structs + DATA_ALIGN;

    // global table
    m_logMvCostTable = videoParam.m_logMvCostTable; 
    
    // m_costStat
    m_costStat = (costStat *)ptr;
    ptr += numCtbs * sizeof(costStat);
    // ----------------------------------------------------

    m_sps        = *(videoParam.csps);
    m_pps        = *(videoParam.cpps);

    m_videoParam.csps = &m_sps;
    m_videoParam.cpps = &m_pps;

    m_videoParam.m_slice_ids = m_slice_ids;
    m_videoParam.m_costStat = m_costStat;

    //// CTB BRC
    //m_videoParam.m_lcuQps = NULL;
    //m_videoParam.m_lcuQps = new Ipp8s[numCtbs];

    ippsZero_8u((Ipp8u *)cu, sizeofH265CU * m_videoParam.num_thread_structs);
    ippsZero_8u((Ipp8u*)data_temp, sizeof(H265CUData) * data_temp_size * m_videoParam.num_thread_structs);

    if (m_videoParam.SAOFlag) {
        m_saoParam.resize(m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs);

        for(Ipp32u idx = 0; idx < m_videoParam.num_thread_structs; idx++) {
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

        for (Ipp32s compId = 0; compId < NUM_USED_SAO_COMPONENTS; compId++) {
            m_saoApplyFilter[compId].Init(m_videoParam.Width, m_videoParam.Height, m_videoParam.MaxCUSize, 0, 
                compId < 1 ? m_videoParam.bitDepthLuma : m_videoParam.bitDepthChroma);
        }
    }

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


void H265FrameEncoder::Close() 
{

    if (m_videoParam.SAOFlag) {
        m_saoParam.resize(0);

        for(Ipp32u idx = 0; idx < m_videoParam.num_thread_structs; idx++) {
            if (m_videoParam.bitDepthLuma == 8) {
                ((H265CU<Ipp8u>*)cu)[idx].m_saoEncodeFilter.Close();
            } else {
                ((H265CU<Ipp16u>*)cu)[idx].m_saoEncodeFilter.Close();
            }
        }

        for (Ipp32s compId = 0; compId < NUM_USED_SAO_COMPONENTS; compId++) {
            m_saoApplyFilter[compId].Close();
        }
    }

    if (memBuf) {
        H265_Free(memBuf);
        memBuf = NULL;
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

} // void H265FrameEncoder::Close()

namespace H265Enc {

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


Ipp8s GetConstQp(Task const & task, H265VideoParam const & param)
{
    const H265Frame *frame = task.m_frameOrigin;
    Ipp8s sliceQpY = param.QPI;
    switch (frame->m_picCodeType)
    {
    case MFX_FRAMETYPE_I:
        return param.QPI;
        break;
    case MFX_FRAMETYPE_P:
        if (param.BiPyramidLayers > 1 && param.longGop && frame->m_biFramesInMiniGop == 15)
            sliceQpY = param.QPI;
        else if (param.PGopPicSize > 1)
            sliceQpY = param.QPP + frame->m_pyramidLayer;
        else
            sliceQpY = param.QPP;
        break;
    case MFX_FRAMETYPE_B:
        if (param.BiPyramidLayers > 1 && param.longGop && frame->m_biFramesInMiniGop == 15)
            sliceQpY = param.QPI + frame->m_pyramidLayer;
        else if (param.BiPyramidLayers > 1)
            sliceQpY = param.QPB + frame->m_pyramidLayer;
        else
            sliceQpY = param.QPB;
        break;
    default:
        // Unsupported Picture Type
        VM_ASSERT(0);
        sliceQpY = param.QPI;
        break;
    }

    return sliceQpY;

} // 

}
mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref)
{
    VM_ASSERT(displayOrder >= begin);
    VM_ASSERT(displayOrder <  end);

    ref = (end - begin > 1);

    mfxU32 pivot = (begin + end) / 2;
    if (displayOrder == pivot)
        return counter;
    else if (displayOrder < pivot)
        return GetEncodingOrder(displayOrder, begin, pivot, counter + 1, ref);
    else
        return GetEncodingOrder(displayOrder, pivot + 1, end, counter + 1 + pivot - begin, ref);
};
void MFXVideoENCODEH265::ConfigureInputFrame(H265Frame* frame) const
{
    frame->m_frameOrder = m_frameOrder;
    frame->m_frameOrderOfLastIdr = m_frameOrderOfLastIdr;
    frame->m_frameOrderOfLastIntra = m_frameOrderOfLastIntra;
    frame->m_frameOrderOfLastAnchor = m_frameOrderOfLastAnchor;
    frame->m_poc = frame->m_frameOrder - frame->m_frameOrderOfLastIdr;
    frame->m_isIdrPic = false;

    Ipp32s idrDist = m_videoParam.GopPicSize * (m_videoParam.IdrInterval + 1);
    if ((frame->m_frameOrder - frame->m_frameOrderOfLastIdr) % idrDist == 0) {
        frame->m_isIdrPic = true;
        frame->m_picCodeType = MFX_FRAMETYPE_I;
        frame->m_poc = 0;
    }
    else if ((frame->m_frameOrder - frame->m_frameOrderOfLastIntra) % m_videoParam.GopPicSize == 0) {
        frame->m_picCodeType = MFX_FRAMETYPE_I;
    }
    else if ((frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist == 0) {
        frame->m_picCodeType = MFX_FRAMETYPE_P;
    }
    else {
        frame->m_picCodeType = MFX_FRAMETYPE_B;
    }

    Ipp64f frameDuration = (Ipp64f)m_videoParam.FrameRateExtD / m_videoParam.FrameRateExtN * 90000;
    if (frame->m_timeStamp == MFX_TIMESTAMP_UNKNOWN)
        frame->m_timeStamp = (m_lastTimeStamp == MFX_TIMESTAMP_UNKNOWN) ? 0 : Ipp64s(m_lastTimeStamp + frameDuration);

    frame->m_RPSIndex = (m_videoParam.GopRefDist > 1)
        ? (frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist
        : (frame->m_frameOrder - frame->m_frameOrderOfLastIntra) % m_videoParam.PGopPicSize;
    frame->m_miniGopCount = m_miniGopCount + !(frame->m_picCodeType == MFX_FRAMETYPE_B);

    if (m_videoParam.PGopPicSize > 1) {
        const Ipp8u PGOP_LAYERS[PGOP_PIC_SIZE] = { 0, 2, 1, 2 };
        frame->m_pyramidLayer = PGOP_LAYERS[frame->m_RPSIndex];
    }

} // void H265FrameEncoder::ConfigureFrame(Ipp32u pictureType)


bool PocIsLess(const H265Frame *f1, const H265Frame *f2) { return f1->m_poc < f2->m_poc; }
bool PocIsGreater(const H265Frame *f1, const H265Frame *f2) { return f1->m_poc > f2->m_poc; }

void MFXVideoENCODEH265::CreateRefPicList(Task *task, H265ShortTermRefPicSet *rps)
{
    H265Frame *currFrame = task->m_frameOrigin;
    H265Frame **list0 = currFrame->m_refPicList[0].m_refFrames;
    H265Frame **list1 = currFrame->m_refPicList[1].m_refFrames;
    Ipp32s currPoc = currFrame->m_poc;
    Ipp8s *dpoc0 = currFrame->m_refPicList[0].m_deltaPoc;
    Ipp8s *dpoc1 = currFrame->m_refPicList[1].m_deltaPoc;

    Ipp32s numStBefore = 0;
    Ipp32s numStAfter = 0;

    // constuct initial reference lists
    task->m_dpbSize = 0;
    for (TaskIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
        H265Frame *ref = (*i)->m_frameRecon;
        task->m_dpb[task->m_dpbSize++] = ref;
        if (ref->m_poc < currFrame->m_poc)
            list0[numStBefore++] = ref;
        else
            list1[numStAfter++] = ref;
    }
    std::sort(list0, list0 + numStBefore, PocIsGreater);
    std::sort(list1, list1 + numStAfter,  PocIsLess);

    // merge lists
    small_memcpy(list0 + numStBefore, list1, sizeof(*list0) * numStAfter);
    small_memcpy(list1 + numStAfter,  list0, sizeof(*list0) * numStBefore);

    // setup delta pocs and long-term flags
    memset(currFrame->m_refPicList[0].m_isLongTermRef, 0, sizeof(currFrame->m_refPicList[0].m_isLongTermRef));
    memset(currFrame->m_refPicList[1].m_isLongTermRef, 0, sizeof(currFrame->m_refPicList[1].m_isLongTermRef));
    for (Ipp32s i = 0; i < numStBefore + numStAfter; i++) {
        dpoc0[i] = Ipp8s(currPoc - list0[i]->m_poc);
        dpoc1[i] = Ipp8s(currPoc - list1[i]->m_poc);
    }

    // cut lists
    if (currFrame->m_picCodeType == MFX_FRAMETYPE_I) {
        currFrame->m_refPicList[0].m_refFramesCount = 0;
        currFrame->m_refPicList[1].m_refFramesCount = 0;
    }
    else if (currFrame->m_picCodeType == MFX_FRAMETYPE_P) {
        currFrame->m_refPicList[0].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, m_videoParam.MaxRefIdxP[0]);
        currFrame->m_refPicList[1].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, m_videoParam.MaxRefIdxP[1]);
    }
    else if (currFrame->m_picCodeType == MFX_FRAMETYPE_B) {
        currFrame->m_refPicList[0].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, m_videoParam.MaxRefIdxB[0]);
        currFrame->m_refPicList[1].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, m_videoParam.MaxRefIdxB[1]);
    }

    // create RPS syntax
//    Ipp32s numL0 = currFrame->m_refPicList[0].m_refFramesCount;
//    Ipp32s numL1 = currFrame->m_refPicList[1].m_refFramesCount;
    rps->inter_ref_pic_set_prediction_flag = 0;
    rps->num_negative_pics = numStBefore;
    rps->num_positive_pics = numStAfter;
    Ipp32s deltaPocPred = 0;
    for (Ipp32s i = 0; i < numStBefore; i++) {
        rps->delta_poc[i] = dpoc0[i] - deltaPocPred;
        //rps->used_by_curr_pic_flag[i] = i < numL0 || i < IPP_MAX(0, numL1 - numStAfter);
        rps->used_by_curr_pic_flag[i] = 1; // mimic current behavior
        deltaPocPred = dpoc0[i];
    }
    deltaPocPred = 0;
    for (Ipp32s i = 0; i < numStAfter; i++) {
        rps->delta_poc[numStBefore + i] = deltaPocPred - dpoc1[i];
        //rps->used_by_curr_pic_flag[numStBefore + i] = i < numL1 || i < IPP_MAX(0, numL0 - numStBefore);
        rps->used_by_curr_pic_flag[numStBefore + i] = 1;
        deltaPocPred = dpoc1[i];
    }
}


Ipp8u SameRps(const H265ShortTermRefPicSet *rps1, const H265ShortTermRefPicSet *rps2)
{
    // check number of refs
    if (rps1->num_negative_pics != rps2->num_negative_pics ||
        rps1->num_positive_pics != rps2->num_positive_pics)
        return 0;
    // check delta pocs and "used" flags
    for (Ipp32s i = 0; i < rps1->num_negative_pics + rps1->num_positive_pics; i++)
        if (rps1->delta_poc[i] != rps2->delta_poc[i] ||
            rps1->used_by_curr_pic_flag[i] != rps2->used_by_curr_pic_flag[i])
            return 0;
    return 1;
}

void MFXVideoENCODEH265::PrepareToEncode(Task* task)
{
    if (m_brc) {
        // to provide bitexact result in case of frame-threading we set Qp here
        // otherwise we will set Qp late (before encode directly) to provide more accurate feedback encode<->brc
        if ( m_videoParam.m_framesInParallel > 1) { 
            task->m_sliceQpY = (Ipp8s)m_brc->GetQP(m_videoParam, task->m_frameOrigin);
        }
    } else {
        task->m_sliceQpY = GetConstQp(*task, m_videoParam);
    }

    //-----------------------------------------------------
    //Ipp32s numCtb = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;
    //memset(&task->m_lcuQps[0], task->m_sliceQpY, sizeof(task->m_sliceQpY)*numCtb);

//#if defined (MFX_ENABLE_H265_PAQ)
//    if(m_videoParam.preEncMode && m_videoParam.UseDQP) {
//        int poc = task->m_frameOrigin->m_poc;
//        for(int ctb=0; ctb<numCtb; ctb++) {
//            int ctb_adapt = ctb;
//            int locPoc = poc % m_preEnc.m_histLength;
//            if(m_videoParam.preEncMode != 2) {// no pure CALQ 
//                task->m_lcuQps[ctb] += m_preEnc.m_acQPMap[locPoc].getDQP(ctb_adapt);
//            }
//        }
//    }
//#endif
//    // ----------------------------------------------------

    H265Frame *currFrame = task->m_frameOrigin;

    // create reference lists and RPS syntax
    H265ShortTermRefPicSet rps = {0};
    CreateRefPicList(task, &rps);
    Ipp32s useSpsRps = SameRps(m_sps.m_shortRefPicSet + currFrame->m_RPSIndex, &rps);

    // setup map list1->list0, used during Motion Estimation
    H265Frame **list0 = currFrame->m_refPicList[0].m_refFrames;
    H265Frame **list1 = currFrame->m_refPicList[1].m_refFrames;
    Ipp32s numRefIdx0 = currFrame->m_refPicList[0].m_refFramesCount;
    Ipp32s numRefIdx1 = currFrame->m_refPicList[1].m_refFramesCount;
    for (Ipp32s idx1 = 0; idx1 < numRefIdx1; idx1++) {
        Ipp32s idxInList0 = (Ipp32s)(std::find(list0, list0 + numRefIdx0, list1[idx1]) - list0);
        currFrame->m_mapRefIdxL1ToL0[idx1] = (idxInList0 < numRefIdx0) ? idxInList0 : -1;
    }
    { 
        // Map to unique
        Ipp32s uniqueIdx = 0;
        for (Ipp32s idx = 0; idx < numRefIdx0; idx++) {
            currFrame->m_mapListRefUnique[0][idx]=uniqueIdx;
            uniqueIdx++;
        }            
        for (Ipp32s idx = numRefIdx0; idx < MAX_NUM_REF_IDX; idx++) {
            currFrame->m_mapListRefUnique[0][idx]=-1;
        }            
        for (Ipp32s idx1 = 0; idx1 < numRefIdx1; idx1++) {
            if(currFrame->m_mapRefIdxL1ToL0[idx1]==-1) {
                currFrame->m_mapListRefUnique[1][idx1]=uniqueIdx;
                uniqueIdx++;
            } else {
                currFrame->m_mapListRefUnique[1][idx1]=currFrame->m_mapListRefUnique[0][currFrame->m_mapRefIdxL1ToL0[idx1]];
            }
        }
        for (Ipp32s idx = numRefIdx1; idx < MAX_NUM_REF_IDX; idx++) {
            currFrame->m_mapListRefUnique[1][idx]=-1;
        }            
        //assert(uniqueIdx<=MAX_NUM_REF_IDX);
        currFrame->m_numRefUnique = uniqueIdx;
    }
    // setup flag m_allRefFramesAreFromThePast used for TMVP
    currFrame->m_allRefFramesAreFromThePast = true;
    for (Ipp32s i = 0; i < numRefIdx0 && currFrame->m_allRefFramesAreFromThePast; i++)
        if (currFrame->m_refPicList[0].m_deltaPoc[i] < 0)
            currFrame->m_allRefFramesAreFromThePast = false;
    for (Ipp32s i = 0; i < numRefIdx1 && currFrame->m_allRefFramesAreFromThePast; i++)
        if (currFrame->m_refPicList[1].m_deltaPoc[i] < 0)
            currFrame->m_allRefFramesAreFromThePast = false;

    // setup slices
    H265Slice *currSlices = task->m_slices;
    for (Ipp8u i = 0; i < m_videoParam.NumSlices; i++) {
        SetSlice(currSlices + i, i, currFrame);

        //-------------------------------------------------
        //(currSlices + i)->slice_qp_delta = task->m_sliceQpY - m_pps.init_qp;
        //SetAllLambda(m_videoParam, (currSlices + i), task->m_sliceQpY, task->m_frameOrigin );
        //--------------------------------------------------

        currSlices[i].short_term_ref_pic_set_sps_flag = useSpsRps;
        if (currSlices[i].short_term_ref_pic_set_sps_flag)
            currSlices[i].short_term_ref_pic_set_idx = currFrame->m_RPSIndex;
        else
            currSlices[i].m_shortRefPicSet = rps;
    }

    task->m_encOrder = currFrame->m_encOrder = m_lastEncOrder + 1;

    // setup ref flags
    currFrame->m_isLongTermRef = 0;
    currFrame->m_isShortTermRef = 1;
    if (currFrame->m_picCodeType == MFX_FRAMETYPE_B) {
        if (m_videoParam.BiPyramidLayers > 1) {
            if (currFrame->m_pyramidLayer == m_videoParam.BiPyramidLayers - 1)
                currFrame->m_isShortTermRef = 0;
        }
        else if (!m_videoParam.TreatBAsReference) {
            currFrame->m_isShortTermRef = 0;
        }
    }
    currFrame->m_isRef = currFrame->m_isShortTermRef | currFrame->m_isLongTermRef;

    // set task param to simplify bitstream writing logic
    {
        task->m_encOrder   = m_lastEncOrder + 1;
        task->m_frameOrder = task->m_frameOrigin->m_frameOrder;
        task->m_timeStamp  = task->m_frameOrigin->m_timeStamp;
        task->m_picCodeType= task->m_frameOrigin->m_picCodeType;
    }

    // assign resource for reconstruct
    FramePtrIter frm = GetFreeFrame(m_freeFrames, &m_videoParam); // calls AddRef()
    task->m_frameRecon = *frm;
    task->m_frameRecon->CopyEncInfo(task->m_frameOrigin);

    // take ownership over reference frames
    for (Ipp32s i = 0; i < task->m_dpbSize; i++)
        task->m_dpb[i]->AddRef();

    UpdateDpb(task);
}

// expect that frames in m_dpb_1 is ordered by m_encOrder
void MFXVideoENCODEH265::UpdateDpb(Task *currTask)
{
    H265Frame *currFrame = currTask->m_frameRecon;
    if (!currFrame->m_isRef)
        return; // non-ref frame doesn't change dpb

    if (currFrame->m_isIdrPic) {
        // IDR frame removes all reference frames from dpb and becomes the only reference
        for (TaskIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
            (*i)->m_frameRecon->Release();
        m_actualDpb.clear();
    }
    else if ((Ipp32s)m_actualDpb.size() == m_videoParam.MaxDecPicBuffering) {
        // dpb is full
        // need to make a place in dpb for a new reference frame
        TaskIter toRemove = m_actualDpb.end();
        if (m_videoParam.GopRefDist > 1) {
            // B frames with or without pyramid
            // search for oldest refs from previous minigop
            Ipp32s currMiniGop = currFrame->m_miniGopCount;
            for (TaskIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
                if ((*i)->m_frameRecon->m_miniGopCount < currMiniGop)
                    if (toRemove == m_actualDpb.end() || (*toRemove)->m_frameRecon->m_poc > (*i)->m_frameRecon->m_poc)
                        toRemove = i;
            if (toRemove == m_actualDpb.end()) {
                // if nothing found
                // search for oldest B frame in current minigop
                for (TaskIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
                    if ((*i)->m_frameRecon->m_picCodeType == MFX_FRAMETYPE_B &&
                        (*i)->m_frameRecon->m_miniGopCount == currMiniGop)
                        if (toRemove == m_actualDpb.end() || (*toRemove)->m_frameRecon->m_poc > (*i)->m_frameRecon->m_poc)
                            toRemove = i;
            }
            if (toRemove == m_actualDpb.end()) {
                // if nothing found
                // search for oldest any oldest ref in current minigop
                for (TaskIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
                    if ((*i)->m_frameRecon->m_miniGopCount == currMiniGop)
                        if (toRemove == m_actualDpb.end() || (*toRemove)->m_frameRecon->m_poc > (*i)->m_frameRecon->m_poc)
                            toRemove = i;
            }
        }
        else {
            // P frames with or without pyramid
            // search for oldest non-anchor ref
            for (TaskIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
                if ((*i)->m_frameRecon->m_pyramidLayer > 0)
                    if (toRemove == m_actualDpb.end() || (*toRemove)->m_frameRecon->m_poc > (*i)->m_frameRecon->m_poc)
                        toRemove = i;
            if (toRemove == m_actualDpb.end()) {
                // if nothing found
                // search for any oldest ref
                for (TaskIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
                    if (toRemove == m_actualDpb.end() || (*toRemove)->m_frameRecon->m_poc > (*i)->m_frameRecon->m_poc)
                        toRemove = i;
            }
        }

        VM_ASSERT(toRemove != m_actualDpb.end());
        (*toRemove)->m_frameRecon->Release();
        m_actualDpb.erase(toRemove);
    }

    VM_ASSERT((Ipp32s)m_actualDpb.size() < m_videoParam.MaxDecPicBuffering);
    m_actualDpb.push_back(currTask);
    currFrame->AddRef();
}


void MFXVideoENCODEH265::CleanTaskPool()
{
    for (TaskIter i = m_dpb.begin(); i != m_dpb.end();) {
        TaskIter curI = i++;
        if ((*curI)->m_frameRecon->m_refCounter == 0)
            m_free.splice(m_free.end(), m_dpb, curI);
    }
}

void MFXVideoENCODEH265::OnEncodingQueried(Task* encoded)
{
    Dump(m_recon_dump_file_name, &m_videoParam, encoded->m_frameRecon, m_dpb);

    // release encoded frame
    encoded->m_frameRecon->Release();

    // release reference frames
    for (Ipp32s i = 0; i < encoded->m_dpbSize; i++)
        encoded->m_dpb[i]->Release();

    // release source frame
    encoded->m_frameOrigin->Release();
    encoded->m_frameOrigin = NULL;

    //printf("\n encOrder = %i codedRow _after_ %i\n", encoded->m_encOrder, encoded->m_frameRecon->m_codedRow);fflush(stderr);
    m_outputQueue.remove(encoded);
    CleanTaskPool();

} // 


template <typename PixType>
mfxStatus H265FrameEncoder::ApplySaoCtuRange(Ipp32u ctuStart, Ipp32u ctuEnd)
{
    IppiSize roiSize;
    roiSize.width = m_videoParam.Width;
    roiSize.height = m_videoParam.Height;

    H265Frame* reconstructFrame = m_task->m_frameRecon;

    PixType* pRec[2] = {(PixType*)reconstructFrame->y, (PixType*)reconstructFrame->uv};
    Ipp32s pitch[3] = {reconstructFrame->pitch_luma_pix, reconstructFrame->pitch_chroma_pix >> 1, reconstructFrame->pitch_chroma_pix >> 1}; // FIXME10BIT
    Ipp32s shift[3] = {0, 1, 1};

    

    for (Ipp32u ctu = ctuStart; ctu < ctuEnd; ctu++) {
        for (Ipp32s compId = 0; compId < NUM_USED_SAO_COMPONENTS; compId++) {

            SaoDecodeFilter<PixType>* sao = (m_videoParam.bitDepthLuma == 8 )
                ? (SaoDecodeFilter<PixType>*)m_saoApplyFilter[compId].m_sao 
                : (SaoDecodeFilter<PixType>*)m_saoApplyFilter[compId].m_sao10bit;

            if (ctu == 0) {
                // save boundaries
                Ipp32s width = roiSize.width >> shift[compId];
                small_memcpy(sao->m_TmpU[1], pRec[compId], sizeof(PixType) * width);
            }

            if (((ctu) % m_videoParam.PicWidthInCtbs) == 0) {
                std::swap(sao->m_TmpU[0], sao->m_TmpU[1]);
            }

            // update #1
            Ipp32s ctb_pelx = ( ctu % m_videoParam.PicWidthInCtbs ) * m_videoParam.MaxCUSize;
            Ipp32s ctb_pely = ( ctu / m_videoParam.PicWidthInCtbs ) * m_videoParam.MaxCUSize;

            Ipp32s offset = (ctb_pelx >> shift[compId]) + (ctb_pely >> shift[compId]) * pitch[compId];

            if ((ctu % m_videoParam.PicWidthInCtbs) == 0 && (ctu / m_videoParam.PicWidthInCtbs) != (m_videoParam.PicHeightInCtbs - 1)) {
                PixType* pRecTmp = pRec[compId] + offset + ( (m_videoParam.MaxCUSize >> shift[compId]) - 1)*pitch[compId];
                small_memcpy(sao->m_TmpU[1], pRecTmp, sizeof(PixType) * (roiSize.width >> shift[compId]) );
            }

            if ((ctu % m_videoParam.PicWidthInCtbs) == 0)
                for (Ipp32u i = 0; i < (m_videoParam.MaxCUSize >> shift[compId])+1; i++)
                    sao->m_TmpL[0][i] = pRec[compId][offset + i*pitch[compId]];

            if ((ctu % m_videoParam.PicWidthInCtbs) != (m_videoParam.PicWidthInCtbs - 1))
                for (Ipp32u i = 0; i < (m_videoParam.MaxCUSize >> shift[compId])+1; i++)
                    sao->m_TmpL[1][i] = pRec[compId][offset + i*pitch[compId] + (m_videoParam.MaxCUSize >> shift[compId])-1];

            if (m_saoParam[ctu][compId].mode_idx != SAO_MODE_OFF) {
                if (m_saoParam[ctu][compId].mode_idx == SAO_MODE_MERGE) {
                    // need to restore SAO parameters
                    //reconstructBlkSAOParam(m_saoParam[ctu], mergeList);
                }
                sao[compId].SetOffsetsLuma(m_saoParam[ctu][compId], m_saoParam[ctu][compId].type_idx);

                h265_ProcessSaoCuOrg_Luma(
                    pRec[compId] + offset,
                    pitch[compId],
                    m_saoParam[ctu][compId].type_idx,
                    sao->m_TmpL[0],
                    &(sao->m_TmpU[0][ctb_pelx >> shift[compId]]),
                    m_videoParam.MaxCUSize >> shift[compId],
                    m_videoParam.MaxCUSize >> shift[compId],
                    roiSize.width >> shift[compId],
                    roiSize.height >> shift[compId],
                    sao->m_OffsetEo,
                    sao->m_OffsetBo,
                    sao->m_ClipTable,
                    ctb_pelx >> shift[compId],
                    ctb_pely >> shift[compId]/*,borders*/);
            }

            std::swap(sao->m_TmpL[0], sao->m_TmpL[1]);            
        }
    }

    return MFX_ERR_NONE;
}


template <typename PixType>
mfxStatus H265FrameEncoder::ApplySaoRow(Ipp32u row)
{
    Ipp32u ctuStart = row*m_videoParam.PicWidthInCtbs;
    Ipp32u ctuEnd   = ctuStart + (m_videoParam.PicWidthInCtbs);

    ApplySaoCtuRange<PixType>(ctuStart, ctuEnd);

    return MFX_ERR_NONE;
}


bool H265FrameEncoder::AllReferencesReady(Ipp32u ctb_row)
{
    Ipp32u refRowLag = m_videoParam.m_lagBehindRefRows;
    H265Frame* currentFrame = m_task->m_frameOrigin;
    H265Slice* slice = m_task->m_slices; // aya: should be &(m_task->m_slices[currentSlice]) 
    // but multislce + parallelFrames don't work together so it is OK now

    for (int list = 0; list < 2; list++) {
        RefPicList* refList = &(currentFrame->m_refPicList[list]);
        for (int refIdx = 0; refIdx < slice->num_ref_idx[list]; refIdx++) {
            H265Frame *ref = refList->m_refFrames[refIdx];
            
            if(ref->m_codedRow == m_videoParam.PicHeightInCtbs) { // all rows were encoded
                continue;
            } else if(ref->m_codedRow < (Ipp32s)(ctb_row + refRowLag)) {
                return false;
            } 
        }
    }

    return true;
}


void H265FrameEncoder::FindJob(Ipp32s & found, Ipp32s & complete, Ipp32u & ctb_row, Ipp32u & ctb_col)
{
    H265VideoParam *pars = &m_videoParam;
    Ipp32u ctb_addr = 0;
    Ipp32u inited_ctb_row = ctb_row;
    
    found = 0; 
    complete = 1;

    ctb_row = 0; 
    ctb_col = 0;

    Ipp32u startRow = m_videoParam.NumSlices > 1 ? 0: m_task->m_frameRecon->m_codedRow; // not implemented effective logic for WPP + multislices

    for (ctb_row = startRow; ctb_row < pars->PicHeightInCtbs; ctb_row++) {
    //for (ctb_row = m_pReconstructFrame->m_codedRow; ctb_row < pars->PicHeightInCtbs; ctb_row++) {

        ctb_col = m_row_info[ctb_row].mt_current_ctb_col + 1;
        if (ctb_col == pars->PicWidthInCtbs) {
            continue;
        }
        
        complete = 0;

        if (m_row_info[ctb_row].mt_busy) {
            continue; // some other thread is working here
        }

        ctb_addr = ctb_row * pars->PicWidthInCtbs + ctb_col;

        if (m_pps.entropy_coding_sync_enabled_flag && ctb_row > 0) {
            Ipp32s nsync = ctb_col < pars->PicWidthInCtbs - 1 ? 1 : 0;
            if ((Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + nsync >= m_task->m_slices[m_slice_ids[ctb_addr]].slice_segment_address) {
                if (m_row_info[ctb_row - 1].mt_current_ctb_col < (Ipp32s)ctb_col + nsync) {
                    continue; // failed dependency check
                }
            }
        }

        // double-check if line is still free and mark it busy
        mfxI32 locked = 0;
        if (m_pps.entropy_coding_sync_enabled_flag)
            locked = vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_busy)), 1, 0);

        if (locked) {
            continue; // some other thread was faster :(
        }

        if  ((Ipp32s)ctb_col != m_row_info[ctb_row].mt_current_ctb_col + 1) {
            vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_busy)), 0, 1);
            continue; // ???

        } else {
            if(m_videoParam.m_framesInParallel == 1) {
                found = 1;
                break; // found line to encode
            } else if(  AllReferencesReady(ctb_row) ) 
            {
                found = 1;
                break; // found line to encode
            } 
            else {
                //m_row_info[ctb_row].mt_busy = 0;
                vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_busy)), 0, 1);
                continue; 
            }
        }

    }

} // 


class OnExitHelper
{
public:
    OnExitHelper(Ipp32u* arg) : m_arg(arg)
    {}
    ~OnExitHelper() 
    { 
        if(m_arg) {
            //printf("\n     exit <=== encOrder = %i thread %i \n", m_task->m_encOrder, ithread);fflush(stderr);
            vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(m_arg), 0, 1);
        }
    }

private:
    Ipp32u* m_arg;
};


template <class PixType> void PadOneLine_v2(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s lineCount, Ipp32s padding)
{
    PixType *ptr = startLine;
    Ipp32s leftPos = 0;
    Ipp32s rightPos = width - 1;

    // left/right
    for (Ipp32s i = 0; i < lineCount; i++, ptr += pitch) {

        for(Ipp32s padIdx = 1; padIdx <= padding; padIdx++) {
            ptr[leftPos-padIdx] = ptr[leftPos];
            ptr[rightPos+padIdx] = ptr[rightPos];
        }
    }
}


IppStatus ippsCopy(const Ipp8u *src, Ipp8u *dst, Ipp32s len)   { return ippsCopy_8u(src, dst, len); }
IppStatus ippsCopy(const Ipp16u *src, Ipp16u *dst, Ipp32s len) { return ippsCopy_16s((const Ipp16s *)src, (Ipp16s *)dst, len); }
IppStatus ippsCopy(const Ipp32u *src, Ipp32u *dst, Ipp32s len) { return ippsCopy_32s((const Ipp32s *)src, (Ipp32s *)dst, len); }

template <class PixType> void PadOneLine_Top_v2(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s padding_w, Ipp32s padding_h)
{
     PixType* ptr = startLine - padding_w;
    for (Ipp32s i = 1; i <= padding_h; i++) {
        ippsCopy(ptr, ptr - i*pitch, width + 2*padding_w);
    }
}


template <class PixType> void PadOneLine_Bottom_v2(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s padding_w, Ipp32s padding_h)
{
    PixType* ptr = startLine - padding_w;

    for (Ipp32s i = 1; i <= padding_h; i++) {
        ippsCopy(ptr, ptr + i*pitch, width + 2*padding_w);
    }
}

// try to process one row by the same thread 
//#define _HOLD_ON_ROW_

template <typename PixType>
mfxStatus H265FrameEncoder::EncodeThread(Ipp32s & ithread, volatile Ipp32u* onExitEvent) 
{
    // Check Flag OnExit - on entry point check
    if ( onExitEvent && (*onExitEvent) >= 2) {
        return MFX_TASK_WORKING;
    }

    if (m_task->m_frameRecon->m_codedRow == m_videoParam.PicHeightInCtbs) {
        return MFX_TASK_DONE;
    }

    H265CU<PixType> *cu = (H265CU<PixType> *)this->cu;
    Ipp32u ctb_row = 0, ctb_col = 0, ctb_addr = 0;
    H265VideoParam *pars = &m_videoParam;
    Ipp8u nz[2];
    H265EncoderRowInfo* row_info = NULL;

    //Ipp32s 
     ithread = -1;

    // assign local thread id
    for(Ipp32u idx = 0; idx < m_videoParam.num_thread_structs; idx++) {
        Ipp32u stage = vm_interlocked_cas32( & m_task->m_ithreadPool[idx], 1, 0);
        if(stage == 0) {
            ithread = idx;
            break;
        }
    }

    if(ithread == -1) {
        return MFX_TASK_BUSY;
    }
    OnExitHelper exitHelper( &(m_task->m_ithreadPool[ithread]) );

    // set poubters
    H265Frame* reconstructFrame = m_task->m_frameRecon;
    H265Frame* m_pCurrentFrame  = m_task->m_frameOrigin;

#if defined (_HOLD_ON_ROW_)
    bool checkCondition = true;
#endif

    while(1) {
        mfxI32 found = 0, complete = 1;

#if defined (_HOLD_ON_ROW_)
        if ( checkCondition )
#endif
        {

            // Check Flag OnExit - per Ctb check
            if ( onExitEvent && (*onExitEvent) >= 2) {
                return MFX_TASK_WORKING;
            }

            {
                //UMC::AutomaticUMCMutex guard(m_guard);
                FindJob(found, complete, ctb_row, ctb_col);
            }

            // make decision
            if (m_task->m_frameRecon->m_codedRow == pars->PicHeightInCtbs) {
                return MFX_TASK_DONE;
            }
            if (complete) {
                break; // all lines are complete (or under processing by other threads ) => frame is encoded
                //return MFX_TASK_BUSY;
            }
            if (!found) { // found no line to encode but there are still incomplete lines
                return MFX_TASK_BUSY;
            }

            ctb_addr = ctb_row * pars->PicWidthInCtbs + ctb_col;
        }

#if defined (_HOLD_ON_ROW_)
        checkCondition = true;
#endif

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "EncodeThread");
        Ipp32s bs_id = ithread;
        if (m_pps.entropy_coding_sync_enabled_flag) {
            bs_id = ctb_row;
        }

        // main routine
        {
            Ipp8u curr_slice = m_slice_ids[ctb_addr];
            Ipp8u end_of_slice_flag = (ctb_addr == m_task->m_slices[curr_slice].slice_address_last_ctb);

            if ((Ipp32s)ctb_addr == m_task->m_slices[curr_slice].slice_segment_address ||
                (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 0)) {

                ippiCABACInit_H265(&m_bs[bs_id].cabacState,
                    m_bs[bs_id].m_base.m_pbsBase,
                    m_bs[bs_id].m_base.m_bitOffset + (Ipp32u)(m_bs[bs_id].m_base.m_pbs - m_bs[bs_id].m_base.m_pbsBase) * 8,
                    m_bs[bs_id].m_base.m_maxBsSize);

                if (m_pps.entropy_coding_sync_enabled_flag && pars->PicWidthInCtbs > 1 &&
                    (Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + 1 >= m_task->m_slices[curr_slice].slice_segment_address) {
                    m_bs[bs_id].CtxRestoreWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * (ctb_row - 1));
                } else {
                    InitializeContextVariablesHEVC_CABAC(m_bs[bs_id].m_base.context_array, 2-m_task->m_slices[curr_slice].slice_type, m_task->m_sliceQpY);
                }

                if (!m_pps.entropy_coding_sync_enabled_flag) {
                    row_info = &(m_task->m_slices[curr_slice].m_row_info);
                    row_info->offset = H265Bs_GetBsSize(&m_bs[bs_id]);
                    row_info->bs_id = bs_id;
                }
            }

            small_memcpy(m_bsf[bs_id].m_base.context_array, m_bs[bs_id].m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);
            m_bsf[bs_id].Reset();

#ifdef DEBUG_CABAC
            printf("\n");
            if (ctb_addr == 0) printf("Start POC %d\n",m_pCurrentFrame->m_PicOrderCnt);
            printf("CTB %d\n",ctb_addr);
#endif

            void *feiOutPtr = NULL;
#if defined (MFX_VA)
            if (m_videoParam.enableCmFlag) {
                FeiContext *m_FeiCtx = reinterpret_cast<FeiContext*>(m_task->m_extParam);
                feiOutPtr = m_FeiCtx->feiH265Out;
            }
#endif // MFX_VA

            cu[ithread].InitCu(pars, reconstructFrame->cu_data + (ctb_addr << pars->Log2NumPartInCU),
                data_temp + ithread * data_temp_size, ctb_addr, (PixType*)reconstructFrame->y,
                (PixType*)reconstructFrame->uv, reconstructFrame->pitch_luma_pix, reconstructFrame->pitch_chroma_pix, m_pCurrentFrame,
                &m_bsf[bs_id], m_task->m_slices + curr_slice, 1, m_logMvCostTable, feiOutPtr, m_task);

            if(m_videoParam.UseDQP && m_videoParam.preEncMode) {
                int deltaQP = -(m_task->m_sliceQpY - m_task->m_lcuQps[ctb_addr]);
                int idxDqp = 2*abs(deltaQP)-((deltaQP<0)?1:0);

                H265Slice* curSlice = &(m_task->m_dqpSlice[idxDqp]);
                cu[ithread].m_rdLambda = curSlice->rd_lambda_slice;
                cu[ithread]. m_rdLambdaSqrt = curSlice->rd_lambda_sqrt_slice;
                cu[ithread].m_ChromaDistWeight = curSlice->ChromaDistWeight_slice;
                cu[ithread].m_rdLambdaInter = curSlice->rd_lambda_inter_slice;
                cu[ithread].m_rdLambdaInterMv = curSlice->rd_lambda_inter_mv_slice;

                /*if(m_videoParam.preEncMode > 1) {
                    int calq = cu[ithread].GetCalqDeltaQp(m_pAQP, m_slices[curr_slice].rd_lambda_slice);
                    m_task->m_lcuQps[ctb_addr] += calq;
                }*/
            }
            
            cu[ithread].GetInitAvailablity();

            cu[ithread].ModeDecision(0, 0);
            //cu[ithread].FillRandom(0, 0);
            //cu[ithread].FillZero(0, 0);

            if (!cu[ithread].HaveChromaRec())
                cu[ithread].EncAndRecChroma(0, 0, 0, NULL);

            bool isRef = m_pCurrentFrame->m_isRef;
            bool doSao = m_sps.sample_adaptive_offset_enabled_flag;
            bool doDbl = !m_task->m_slices[curr_slice].slice_deblocking_filter_disabled_flag;

            if (doDbl && (isRef || doSao /*|| m_recon_dump_file_name*/)) {
                if (pars->UseDQP)
                    cu[ithread].UpdateCuQp();
                cu[ithread].Deblock();
            }

            if (m_sps.sample_adaptive_offset_enabled_flag) {
                EstimateCtuSao<PixType>(ithread, ctb_row, ctb_addr, curr_slice);
            }

            cu[ithread].EncodeCU(&m_bs[bs_id], 0, 0, 0);

            if (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 1)
                m_bs[bs_id].CtxSaveWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * ctb_row);

            m_bs[bs_id].EncodeSingleBin_CABAC(CTX(&m_bs[bs_id],END_OF_SLICE_FLAG_HEVC), end_of_slice_flag);


            if ((m_pps.entropy_coding_sync_enabled_flag && ctb_col == pars->PicWidthInCtbs - 1) ||
                end_of_slice_flag) {
#ifdef DEBUG_CABAC
                int d = DEBUG_CABAC_PRINT;
                DEBUG_CABAC_PRINT = 1;
#endif
                if (!end_of_slice_flag)
                    m_bs[bs_id].EncodeSingleBin_CABAC(CTX(&m_bs[bs_id],END_OF_SLICE_FLAG_HEVC), 1);
                m_bs[bs_id].TerminateEncode_CABAC();
                m_bs[bs_id].ByteAlignWithZeros();
#ifdef DEBUG_CABAC
                DEBUG_CABAC_PRINT = d;
#endif
                // ----------------------------------------
                if (!m_pps.entropy_coding_sync_enabled_flag) {
                    H265EncoderRowInfo *row_info = &(m_task->m_slices[curr_slice].m_row_info);
                    row_info->bs_id = bs_id;
                    row_info->size = H265Bs_GetBsSize(&m_bs[bs_id]) - row_info->offset;
                }
                //-----------------------------------------
            }
            
            if (ctb_col == pars->PicWidthInCtbs - 1) {

                if (m_pCurrentFrame->IsReference()) {
                    if (ctb_row > 0) {
                        if (m_sps.sample_adaptive_offset_enabled_flag) 
                            ApplySaoRow<PixType>(ctb_row-1);
                        PadOneReconRow<PixType>(ctb_row - 1);
                    }
                    if( ctb_row == pars->PicHeightInCtbs - 1 ) { //special case for last row
                        if (m_sps.sample_adaptive_offset_enabled_flag) 
                            ApplySaoRow<PixType>(ctb_row);
                        PadOneReconRow<PixType>(ctb_row);
                    }
                }

                // increment to signal enother encoder
                vm_interlocked_inc32( reinterpret_cast<volatile Ipp32u *> (&(reconstructFrame->m_codedRow)) );

                vm_interlocked_inc32( reinterpret_cast<volatile Ipp32u *> (&(m_row_info[ctb_row].mt_current_ctb_col)) );
                vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_busy)), 0, 1);

            } else {

                vm_interlocked_inc32( reinterpret_cast<volatile Ipp32u *> (&(m_row_info[ctb_row].mt_current_ctb_col)) );

#ifndef _HOLD_ON_ROW_
                vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_busy)), 0, 1);
#else

                if ( onExitEvent && (*onExitEvent) >= 2) {
                    vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_busy)), 0, 1);
                    return MFX_TASK_WORKING;
                }

                ctb_col = m_row_info[ctb_row].mt_current_ctb_col + 1;
                ctb_addr = ctb_row * pars->PicWidthInCtbs + ctb_col;
                
                // dependency check
                checkCondition = false;
                if (m_pps.entropy_coding_sync_enabled_flag && ctb_row > 0) {
                    Ipp32s nsync = ctb_col < pars->PicWidthInCtbs - 1 ? 1 : 0;
                    if ((Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + nsync >= m_task->m_slices[m_slice_ids[ctb_addr]].slice_segment_address) {
                        if (m_row_info[ctb_row - 1].mt_current_ctb_col < (Ipp32s)ctb_col + nsync) { // failed dependency check
                            vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_busy)), 0, 1);
                            checkCondition = true; // try to find next task (ctb) in current frame
                        }
                    }
                }
#endif
            }
        }
    }

    return MFX_TASK_DONE;
}


template <typename PixType>
void H265FrameEncoder::EstimateCtuSao(Ipp32s ithread, Ipp32u ctb_row, Ipp32u ctb_addr, Ipp8u curr_slice)
{
    H265CU<PixType> *cu = (H265CU<PixType> *)this->cu;

#if defined(MFX_HEVC_SAO_PREDEBLOCKED_ENABLED)
                Ipp32s left_addr  = cu[ithread].left_addr;
                Ipp32s above_addr = cu[ithread].above_addr;
                MFX_HEVC_PP::CTBBorders borders = {0};

                borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

                cu[ithread].GetStatisticsCtuSaoPredeblocked( borders );
#endif
                Ipp32s left_addr = cu[ithread].m_leftAddr;
                Ipp32s above_addr = cu[ithread].m_aboveAddr;

                MFX_HEVC_PP::CTBBorders borders = {0};

                borders.m_left  = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                borders.m_top = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

    // aya: here should be slice lambda always
    cu[ithread].m_rdLambda = m_task->m_slices[curr_slice].rd_lambda_slice;

    cu[ithread].EstimateCtuSao(&m_bsf[ctb_row], &m_saoParam[ctb_addr], &m_saoParam[0], borders, m_slice_ids);

                // aya: tiles issues???
                borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[left_addr]) ? 1 : 0;
                borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[above_addr]) ? 1 : 0;

                bool leftMergeAvail  = borders.m_left > 0 ? true : false;
                bool aboveMergeAvail = borders.m_top  > 0 ? true : false;

    cu[ithread].EncodeSao(&m_bs[ctb_row], 0, 0, 0, m_saoParam[ctb_addr], leftMergeAvail, aboveMergeAvail);

    cu[ithread].m_saoEncodeFilter.ReconstructCtuSaoParam(m_saoParam[ctb_addr]);

}


template <typename PixType>
void H265FrameEncoder::PadOneReconRow(Ipp32u ctb_row)
{
    H265Frame *reconstructFrame = m_task->m_frameRecon;

    Ipp8u bitDepth8 = (m_videoParam.bitDepthLuma == 8) ? 1 : 0;
    Ipp32s shift_w = reconstructFrame->m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
    Ipp32s shift_h = reconstructFrame->m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;

    // L/R Padding
    {
        //luma
        Ipp8u *startLine = reconstructFrame->y + (ctb_row)*reconstructFrame->pitch_luma_bytes*m_videoParam.MaxCUSize;
        PadOneLine_v2((PixType*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, m_videoParam.MaxCUSize, reconstructFrame->padding);

        //Chroma
        //startLine = reconstructFrame->uv + (ctb_row)*reconstructFrame->pitch_chroma_bytes* m_videoParam.MaxCUSize/2;
        startLine = reconstructFrame->uv + (ctb_row)*reconstructFrame->pitch_chroma_bytes* (m_videoParam.MaxCUSize >> shift_h);
        (bitDepth8) 
            ? PadOneLine_v2((Ipp16u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, m_videoParam.MaxCUSize >> shift_h, reconstructFrame->padding >> shift_w)
            : PadOneLine_v2((Ipp32u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, m_videoParam.MaxCUSize >> shift_h, reconstructFrame->padding >> shift_w);
    }

    //first line: top padding
    if (ctb_row == 0) {
        Ipp8u *startLine = reconstructFrame->y;
        PadOneLine_Top_v2((PixType*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, reconstructFrame->padding, reconstructFrame->padding);

        startLine = reconstructFrame->uv;
        (bitDepth8)
            ? PadOneLine_Top_v2((Ipp16u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h)
            : PadOneLine_Top_v2((Ipp32u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h);
    }

    // last line: bottom padding
    if (ctb_row == m_videoParam.PicHeightInCtbs - 1) {
        Ipp8u *startLine = reconstructFrame->y + (reconstructFrame->height - 1)*reconstructFrame->pitch_luma_bytes;
        PadOneLine_Bottom_v2((PixType*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, reconstructFrame->padding, reconstructFrame->padding);

        startLine = reconstructFrame->uv + ((reconstructFrame->height >> shift_h) - 1)*reconstructFrame->pitch_chroma_bytes;
        (bitDepth8)
            ? PadOneLine_Bottom_v2((Ipp16u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h)
            : PadOneLine_Bottom_v2((Ipp32u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h);
    }
}


// no thread safe !!!
mfxStatus H265FrameEncoder::SetEncodeTask(Task* task) 
{ 
    MFX_CHECK_NULL_PTR1(task);

    m_task = (Task*)task;

    for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
        H265Slice *slice = m_task->m_slices + curr_slice;
        for (Ipp32u i = slice->slice_segment_address; i <= slice->slice_address_last_ctb; i++)
            m_slice_ids[i] = curr_slice;
    }

    m_task->m_frameRecon->m_codedRow = 0;
    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs; i++) {
        m_bs[i].Reset();
    }
    
    for (Ipp32u i = 0; i < m_videoParam.PicHeightInCtbs; i++) {
        m_row_info[i].mt_current_ctb_col = -1;
        m_row_info[i].mt_busy = 0;
    }

    return MFX_ERR_NONE; 
}


#if defined (MFX_ENABLE_H265_PAQ)

H265Frame*  findOldestToLookAheadProcess(std::list<Task*> & queue)
{
    if ( queue.empty() ) 
        return NULL;

    for (TaskIter it = queue.begin(); it != queue.end(); it++ ) {

        if ( !(*it)->m_frameOrigin->wasLAProcessed() ) {
            return (*it)->m_frameOrigin;
        }
    }

    return NULL;
}

mfxStatus MFXVideoENCODEH265::PreEncAnalysis(void)
{
    H265Frame* curFrameInEncodeOrder = findOldestToLookAheadProcess(m_inputQueue);

    if(curFrameInEncodeOrder) {
        curFrameInEncodeOrder->setWasLAProcessed();
    }
    if(curFrameInEncodeOrder) {
        mfxFrameSurface1 inputSurface;
        inputSurface.Info.Width = curFrameInEncodeOrder->width;
        inputSurface.Info.Height = curFrameInEncodeOrder->height;
        inputSurface.Data.Pitch = curFrameInEncodeOrder->pitch_luma_pix;
        inputSurface.Data.Y = curFrameInEncodeOrder->y;

        m_preEnc.m_inputSurface = &inputSurface;
        bool bStatus = m_preEnc.preAnalyzeOne(m_preEnc.m_acQPMap);
    } else {
        m_preEnc.m_inputSurface = NULL;
        bool bStatus = m_preEnc.preAnalyzeOne(m_preEnc.m_acQPMap); 
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::UpdateAllLambda(Task* task)
{
    H265Frame* frame = task->m_frameOrigin;

    int  origQP = task->m_sliceQpY;
    for(int iDQpIdx = 0; iDQpIdx < 2*(MAX_DQP)+1; iDQpIdx++)  {
        int deltaQP = ((iDQpIdx+1)>>1)*(iDQpIdx%2 ? -1 : 1);
        int curQp = origQP + deltaQP;

        task->m_dqpSlice[iDQpIdx].slice_type = task->m_slices[0].slice_type;

        bool IsHiCplxGOP = false;
        bool IsMedCplxGOP = false;
        if (2 == m_videoParam.preEncMode ) {
            TQPMap *pcQPMapPOC = &m_preEnc.m_acQPMap[frame->m_poc % m_preEnc.m_histLength];
            double SADpp = pcQPMapPOC->getAvgGopSADpp();
            double SCpp  = pcQPMapPOC->getAvgGopSCpp();
            if (SCpp > 2.0) {
                double minSADpp = 0;
                if (m_videoParam.GopRefDist > 8) {
                    minSADpp = 1.3*SCpp - 2.6;
                    if (minSADpp>0 && minSADpp<SADpp) IsHiCplxGOP = true;
                    if (!IsHiCplxGOP) {
                        double minSADpp = 1.1*SCpp - 2.2;
                        if(minSADpp>0 && minSADpp<SADpp) IsMedCplxGOP = true;
                    }
                } 
                else {
                    minSADpp = 1.1*SCpp - 1.5;
                    if (minSADpp>0 && minSADpp<SADpp) IsHiCplxGOP = true;
                    if (!IsHiCplxGOP) {
                        double minSADpp = 1.0*SCpp - 2.0;
                        if (minSADpp>0 && minSADpp<SADpp) IsMedCplxGOP = true;
                    }
                }
            }
        }

        SetAllLambda(m_videoParam, &(task->m_dqpSlice[iDQpIdx]), curQp, frame, IsHiCplxGOP, IsMedCplxGOP);
    }

    if ( m_videoParam.preEncMode > 1 ) {
        m_pAQP->m_POC = frame->m_poc;
        m_pAQP->m_SliceType = B_SLICE;
        if (MFX_FRAMETYPE_I == frame->m_picCodeType) {
            m_pAQP->m_SliceType = I_SLICE;
        }

        m_pAQP->set_pic_coding_class(frame->m_pyramidLayer + 1);
        m_pAQP->setSliceQP(task->m_sliceQpY);
    }

    return MFX_ERR_NONE;
}


template <typename PixType>
double compute_block_rscs(PixType *pSrcBuf, int width, int height, int stride)
{
    int     i, j;
    int     tmpVal, CsVal, RsVal;
    int     blkSizeC = (width-1) * height;
    int     blkSizeR = width * (height-1);
    double  RS, CS, RsCs;
    PixType *pBuf;

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

    if(blkSizeC) {
        CS = sqrt((double)CsVal / (double)blkSizeC);
    }
    else {
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
}

template <typename PixType>
int H265CU<PixType>::GetCalqDeltaQp(TAdapQP* sliceAQP, Ipp64f sliceLambda)
{
    TAdapQP ctbAQP;

    ctbAQP.m_cuAddr = m_ctbAddr;
    ctbAQP.m_Xcu    = m_ctbPelX;
    ctbAQP.m_Ycu    = m_ctbPelY;

    if((ctbAQP.m_Xcu + sliceAQP->m_maxCUWidth) > (int)sliceAQP->m_picWidth)  ctbAQP.m_cuWidth = sliceAQP->m_picWidth - ctbAQP.m_Xcu;
    else ctbAQP.m_cuWidth = sliceAQP->m_maxCUWidth;

    if((ctbAQP.m_Ycu + sliceAQP->m_maxCUHeight) > (int)sliceAQP->m_picHeight)  ctbAQP.m_cuHeight = sliceAQP->m_picHeight - ctbAQP.m_Ycu;
    else ctbAQP.m_cuHeight = sliceAQP->m_maxCUHeight;

    ctbAQP.m_rscs =  compute_block_rscs(m_ySrc, ctbAQP.m_cuWidth, ctbAQP.m_cuHeight, m_pitchSrcLuma);

    ctbAQP.m_GOPSize = sliceAQP->m_GOPSize;
    ctbAQP.m_picClass = sliceAQP->m_picClass;
    ctbAQP.setSliceQP(sliceAQP->m_sliceQP);
    ctbAQP.setClass_RSCS(ctbAQP.m_rscs);

    int calq = (ctbAQP.getQPFromLambda(sliceLambda * 256.0) - sliceAQP->m_sliceQP);
    return calq;
}

#else

mfxStatus H265Encoder::PreEncAnalysis(mfxBitstream* mfxBS)
{
    mfxBS; return MFX_ERR_NONE;

}
mfxStatus H265Encoder::UpdateAllLambda(H265Frame* frame)
{
    frame;
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

template mfxStatus H265FrameEncoder::EncodeThread<Ipp8u>(Ipp32s & ithread, volatile Ipp32u* onExitEvent);
template mfxStatus H265FrameEncoder::EncodeThread<Ipp16u>(Ipp32s & ithread, volatile Ipp32u* onExitEvent);

//} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
