//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2015 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <new>
#include <numeric>
#include <stdexcept>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <algorithm>

#include "mfxdefs.h"
#include "mfx_task.h"
#include "vm_interlocked.h"
#include "vm_cond.h"
#include "vm_file.h"

#include "mfx_h265_encode.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_lookahead.h"
#include "mfx_enc_common.h"
#include "mfx_h265_fei.h"
#include "mfx_h265_enc_cm_utils.h"
#include "mfx_h265_cmcopy.h"

#ifndef MFX_VA
#define H265FEI_Close(...) (MFX_ERR_NONE)
#define H265FEI_GetSurfaceDimensions(...) (MFX_ERR_NONE)
#define H265FEI_GetSurfaceDimensions_new(...) (MFX_ERR_NONE)
#define H265FEI_Init(...) (MFX_ERR_NONE)
#define H265FEI_ProcessFrameAsync(...) (MFX_ERR_NONE)
#define H265FEI_SyncOperation(...) (MFX_ERR_NONE)
#define H265FEI_DestroySavedSyncPoint(...) (MFX_ERR_NONE)
#endif

using namespace H265Enc;
using namespace H265Enc::MfxEnumShortAliases;

#define TT_TRACE 1

namespace {
#if TT_TRACE
    const char *TT_NAMES[] = {
        "NEWF",
        "PAD",
        "SBMT",
        "WAIT",
        "ENC",
        "PP",
        "PP_ROW",
        "ECOMP",
        "HUB",
        "LA_START",
        "LA",
        "LA_END",
        "COMP"
    };

    const char *GetFeiOpName(Ipp32s feiOp) {
        switch (feiOp) {
        case MFX_FEI_H265_OP_NOP:               return "NOP";
        case MFX_FEI_H265_OP_GPU_COPY_SRC:      return "CPSRC";
        case MFX_FEI_H265_OP_GPU_COPY_REF:      return "CPREF";
        case MFX_FEI_H265_OP_INTRA_MODE:        return "GRAD";
        case MFX_FEI_H265_OP_INTRA_DIST:        return "IDIST";
        case MFX_FEI_H265_OP_INTER_ME:          return "ME";
        case MFX_FEI_H265_OP_INTERPOLATE:       return "INTERP";
        case MFX_FEI_H265_OP_POSTPROC:          return "PP";
        case MFX_FEI_H265_OP_DEBLOCK:           return "PP_DBLK";
        default:                                return "<unk>";
        }
    }

    const char *NUMBERS[128] = {
        "000", "001", "002", "003", "004", "005", "006", "007", "008", "009", "010", "011", "012", "013", "014", "015",
        "016", "017", "018", "019", "020", "021", "022", "023", "024", "025", "026", "027", "028", "029", "030", "031",
        "032", "033", "034", "035", "036", "037", "038", "039", "040", "041", "042", "043", "044", "045", "046", "047",
        "048", "049", "050", "051", "052", "053", "054", "055", "056", "057", "058", "059", "060", "061", "062", "063",
        "064", "065", "066", "067", "068", "069", "070", "071", "072", "073", "074", "075", "076", "077", "078", "079",
        "080", "081", "082", "083", "084", "085", "086", "087", "088", "089", "090", "091", "092", "093", "094", "095",
        "096", "097", "098", "099", "100", "101", "102", "103", "104", "105", "106", "107", "108", "109", "110", "111",
        "112", "113", "114", "115", "116", "117", "118", "119", "120", "121", "122", "123", "124", "125", "126", "127"
    };
#endif //TT_TRACE

    const Ipp32s MYRAND_MAX = 0xffff;
    Ipp32s myrand()
    {
        // feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1
        static Ipp16u val = 0xACE1u;
        Ipp32u bit = ((val >> 0) ^ (val >> 2) ^ (val >> 3) ^ (val >> 5)) & 1;
        val = (val >> 1) | (bit << 15);
        return val;
    }

    const Ipp64f tab_defaultLambdaCorrIP[8]   = {1.0, 1.06, 1.09, 1.13, 1.17, 1.25, 1.27, 1.28};

    void ConvertToInternalParam(H265VideoParam &intParam, const mfxVideoParam &mfxParam)
    {
        const mfxInfoMFX &mfx = mfxParam.mfx;
        const mfxFrameInfo &fi = mfx.FrameInfo;
        const mfxExtOpaqueSurfaceAlloc &opaq = GetExtBuffer(mfxParam);
        const mfxExtCodingOptionHEVC &optHevc = GetExtBuffer(mfxParam);
        const mfxExtHEVCRegion &region = GetExtBuffer(mfxParam);
        const mfxExtHEVCTiles &tiles = GetExtBuffer(mfxParam);
        const mfxExtHEVCParam &hevcParam = GetExtBuffer(mfxParam);
        mfxExtDumpFiles &dumpFiles = GetExtBuffer(mfxParam);
        const mfxExtCodingOption &opt = GetExtBuffer(mfxParam);
        const mfxExtCodingOption2 &opt2 = GetExtBuffer(mfxParam);
        const mfxExtEncoderROI &roi = GetExtBuffer(mfxParam);

        const Ipp32u maxCUSize = 1 << optHevc.Log2MaxCUSize;

        Zero(intParam);
        intParam.encodedOrder = (Ipp8u)mfxParam.mfx.EncodedOrder;

        intParam.fourcc = fi.FourCC;
        intParam.bitDepthLuma = 8;
        intParam.bitDepthChroma = 8;
        intParam.chromaFormatIdc = MFX_CHROMAFORMAT_YUV420;
        if (fi.FourCC == MFX_FOURCC_P010 || fi.FourCC == MFX_FOURCC_P210) {
            intParam.bitDepthLuma = 10;
            intParam.bitDepthChroma = 10;
        }
        intParam.chromaFormatIdc = fi.ChromaFormat;
        intParam.bitDepthLumaShift = intParam.bitDepthLuma - 8;
        intParam.bitDepthChromaShift = intParam.bitDepthChroma - 8;
        intParam.chroma422 = (intParam.chromaFormatIdc == MFX_CHROMAFORMAT_YUV422);
        intParam.chromaShiftW = (intParam.chromaFormatIdc != MFX_CHROMAFORMAT_YUV444);
        intParam.chromaShiftH = (intParam.chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);
        intParam.chromaShiftWInv = 1 - intParam.chromaShiftW;
        intParam.chromaShiftHInv = 1 - intParam.chromaShiftH;
        intParam.chromaShift = intParam.chromaShiftW + intParam.chromaShiftH;

        intParam.Log2MaxCUSize = optHevc.Log2MaxCUSize;
        intParam.MaxCUDepth = optHevc.MaxCUDepth;
        intParam.QuadtreeTULog2MaxSize = optHevc.QuadtreeTULog2MaxSize;
        intParam.QuadtreeTULog2MinSize = optHevc.QuadtreeTULog2MinSize;
        intParam.QuadtreeTUMaxDepthIntra = optHevc.QuadtreeTUMaxDepthIntra;
        intParam.QuadtreeTUMaxDepthInter = optHevc.QuadtreeTUMaxDepthInter;
        intParam.QuadtreeTUMaxDepthInterRD = optHevc.QuadtreeTUMaxDepthInterRD;
        intParam.partModes = optHevc.PartModes;
        intParam.TMVPFlag = (optHevc.TMVP == ON);
        intParam.QPI = (Ipp8s)(mfx.QPI - optHevc.ConstQpOffset);
        intParam.QPP = (Ipp8s)(mfx.QPP - optHevc.ConstQpOffset);
        intParam.QPB = (Ipp8s)(mfx.QPB - optHevc.ConstQpOffset);

        intParam.GopPicSize = mfx.GopPicSize;
        intParam.GopRefDist = mfx.GopRefDist;
        intParam.IdrInterval = mfx.IdrInterval;
        intParam.GopClosedFlag = !!(mfx.GopOptFlag & MFX_GOP_CLOSED);
        intParam.GopStrictFlag = !!(mfx.GopOptFlag & MFX_GOP_STRICT);
        intParam.BiPyramidLayers = (optHevc.BPyramid == ON) ? (H265_CeilLog2(intParam.GopRefDist) + 1) : 1;

        const Ipp32s numRefFrame = MIN(4, mfx.NumRefFrame);
        intParam.longGop = (optHevc.BPyramid == ON && intParam.GopRefDist == 16 && numRefFrame == 5);
        intParam.GeneralizedPB = (optHevc.GPB == ON);
        intParam.AdaptiveRefs = (optHevc.AdaptiveRefs == ON);
        intParam.NumRefFrameB  = optHevc.NumRefFrameB;
        if (intParam.NumRefFrameB < 2)
            intParam.NumRefFrameB = numRefFrame;
        intParam.NumRefLayers  = optHevc.NumRefLayers;
        if (intParam.NumRefLayers < 2 || intParam.BiPyramidLayers<2)
            intParam.NumRefLayers = 2;
        intParam.IntraMinDepthSC = (Ipp8u)optHevc.IntraMinDepthSC - 1;
        intParam.InterMinDepthSTC = (Ipp8u)optHevc.InterMinDepthSTC - 1;
        intParam.MotionPartitionDepth = (Ipp8u)optHevc.MotionPartitionDepth - 1;
        intParam.MaxDecPicBuffering = MAX(numRefFrame, intParam.BiPyramidLayers);
        intParam.MaxRefIdxP[0] = numRefFrame;
        intParam.MaxRefIdxP[1] = intParam.GeneralizedPB ? numRefFrame : 0;
        intParam.MaxRefIdxB[0] = 0;
        intParam.MaxRefIdxB[1] = 0;

        if (intParam.GopRefDist > 1) {
            if (intParam.longGop) {
                intParam.MaxRefIdxB[0] = 2;
                intParam.MaxRefIdxB[1] = 2;
            }
            else if (intParam.BiPyramidLayers > 1) {
                Ipp16u NumRef = IPP_MIN(intParam.NumRefFrameB, numRefFrame);
                intParam.MaxRefIdxB[0] = (NumRef + 1) / 2;
                intParam.MaxRefIdxB[1] = IPP_MAX(1, (NumRef + 0) / 2);
            } else {
                intParam.MaxRefIdxB[0] = numRefFrame - 1;
                intParam.MaxRefIdxB[1] = 1;
            }
        }

        intParam.PGopPicSize = (intParam.GopRefDist == 1 && intParam.MaxRefIdxP[0] > 1) ? PGOP_PIC_SIZE : 1;

        intParam.AnalyseFlags = 0;
        if (optHevc.AnalyzeChroma == ON) intParam.AnalyseFlags |= HEVC_ANALYSE_CHROMA;
        if (optHevc.CostChroma == ON)    intParam.AnalyseFlags |= HEVC_COST_CHROMA;

        intParam.SplitThresholdStrengthCUIntra = (Ipp8u)optHevc.SplitThresholdStrengthCUIntra - 1;
        intParam.SplitThresholdStrengthTUIntra = (Ipp8u)optHevc.SplitThresholdStrengthTUIntra - 1;
        intParam.SplitThresholdStrengthCUInter = (Ipp8u)optHevc.SplitThresholdStrengthCUInter - 1;
        intParam.SplitThresholdTabIndex        = (Ipp8u)optHevc.SplitThresholdTabIndex;

        intParam.FastInterp = (optHevc.FastInterp == ON);
        intParam.cpuFeature = optHevc.CpuFeature;
        intParam.IntraChromaRDO = (optHevc.IntraChromaRDO == ON);
        intParam.SBHFlag = (optHevc.SignBitHiding == ON);
        intParam.RDOQFlag = (optHevc.RDOQuant == ON);
        intParam.rdoqChromaFlag = (optHevc.RDOQuantChroma == ON);
        intParam.FastCoeffCost = (optHevc.FastCoeffCost == ON);
        intParam.rdoqCGZFlag = (optHevc.RDOQuantCGZ == ON);
        intParam.SAOFlag = (optHevc.SAO == ON);
        intParam.SAOChromaFlag = (optHevc.SAOChroma == ON);
        intParam.num_threads = mfx.NumThread;

        if(intParam.NumRefLayers==4) {
            intParam.refLayerLimit[0] = 0;
            intParam.refLayerLimit[1] = 1;
            intParam.refLayerLimit[2] = 1;
            intParam.refLayerLimit[3] = 2;
        } else if(intParam.NumRefLayers==3) {
            intParam.refLayerLimit[0] = 1;
            intParam.refLayerLimit[1] = 1;
            intParam.refLayerLimit[2] = 1;
            intParam.refLayerLimit[3] = 2;
        } else {
            intParam.refLayerLimit[0] = 4;
            intParam.refLayerLimit[1] = 4;
            intParam.refLayerLimit[2] = 4;
            intParam.refLayerLimit[3] = 4;
        }

        intParam.num_cand_0[0][2] = (Ipp8u)optHevc.IntraNumCand0_2;
        intParam.num_cand_0[0][3] = (Ipp8u)optHevc.IntraNumCand0_3;
        intParam.num_cand_0[0][4] = (Ipp8u)optHevc.IntraNumCand0_4;
        intParam.num_cand_0[0][5] = (Ipp8u)optHevc.IntraNumCand0_5;
        intParam.num_cand_0[0][6] = (Ipp8u)optHevc.IntraNumCand0_6;
        Copy(intParam.num_cand_0[1], intParam.num_cand_0[0]);
        Copy(intParam.num_cand_0[2], intParam.num_cand_0[0]);
        Copy(intParam.num_cand_0[3], intParam.num_cand_0[0]);
        intParam.num_cand_1[2] = (Ipp8u)optHevc.IntraNumCand1_2;
        intParam.num_cand_1[3] = (Ipp8u)optHevc.IntraNumCand1_3;
        intParam.num_cand_1[4] = (Ipp8u)optHevc.IntraNumCand1_4;
        intParam.num_cand_1[5] = (Ipp8u)optHevc.IntraNumCand1_5;
        intParam.num_cand_1[6] = (Ipp8u)optHevc.IntraNumCand1_6;
        intParam.num_cand_2[2] = (Ipp8u)optHevc.IntraNumCand2_2;
        intParam.num_cand_2[3] = (Ipp8u)optHevc.IntraNumCand2_3;
        intParam.num_cand_2[4] = (Ipp8u)optHevc.IntraNumCand2_4;
        intParam.num_cand_2[5] = (Ipp8u)optHevc.IntraNumCand2_5;
        intParam.num_cand_2[6] = (Ipp8u)optHevc.IntraNumCand2_6;

        intParam.LowresFactor = optHevc.LowresFactor - 1;
        intParam.DeltaQpMode  = optHevc.DeltaQpMode  - 1;
#ifdef AMT_DQP_FIX
        intParam.LambdaCorrection = tab_defaultLambdaCorrIP[(intParam.DeltaQpMode&AMT_DQP_CAQ)?mfx.TargetUsage:0];
#endif

        intParam.SceneCut = (opt2.AdaptiveI == ON);
        intParam.AnalyzeCmplx = optHevc.AnalyzeCmplx - 1;
        intParam.RateControlDepth = optHevc.RateControlDepth;
        intParam.TryIntra = optHevc.TryIntra;
        intParam.FastAMPSkipME = optHevc.FastAMPSkipME;
        intParam.FastAMPRD = optHevc.FastAMPRD;
        intParam.SkipMotionPartition = optHevc.SkipMotionPartition;
        intParam.cmIntraThreshold = optHevc.CmIntraThreshold;
        intParam.tuSplitIntra = optHevc.TUSplitIntra;
        intParam.cuSplit = optHevc.CUSplit;
        intParam.intraAngModes[0] = optHevc.IntraAngModes;
        intParam.intraAngModes[1] = optHevc.IntraAngModesP;
        intParam.intraAngModes[2] = optHevc.IntraAngModesBRef;
        intParam.intraAngModes[3] = optHevc.IntraAngModesBnonRef;
        intParam.fastSkip = (optHevc.FastSkip == ON);
        intParam.SkipCandRD = (optHevc.SkipCandRD == ON);
        intParam.fastCbfMode = (optHevc.FastCbfMode == ON);
        intParam.hadamardMe = optHevc.HadamardMe;
        intParam.TMVPFlag = (optHevc.TMVP == ON);
        intParam.deblockingFlag = (optHevc.Deblocking == ON);
        intParam.deblockBordersFlag = (intParam.deblockingFlag && optHevc.DeblockBorders == ON);
        intParam.saoOpt = optHevc.SaoOpt;
        intParam.saoSubOpt = optHevc.SaoSubOpt;
        intParam.patternIntPel = optHevc.PatternIntPel;
        intParam.patternSubPel = optHevc.PatternSubPel;
        intParam.numBiRefineIter = optHevc.NumBiRefineIter;
        intParam.puDecisionSatd = (optHevc.PuDecisionSatd == ON);
        intParam.minCUDepthAdapt = (optHevc.MinCUDepthAdapt == ON);
        intParam.maxCUDepthAdapt = (optHevc.MaxCUDepthAdapt == ON);
        intParam.cuSplitThreshold = optHevc.CUSplitThreshold;

        intParam.MaxTrSize = 1 << intParam.QuadtreeTULog2MaxSize;
        intParam.MaxCUSize = maxCUSize;

        intParam.enableCmFlag = (optHevc.EnableCm == ON);
        intParam.enableCmPostProc = intParam.enableCmFlag && (intParam.bitDepthLuma == 8) && (intParam.chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);

        intParam.m_framesInParallel = optHevc.FramesInParallel;
        // intParam.m_lagBehindRefRows = 3;
        // intParam.m_meSearchRangeY = (intParam.m_lagBehindRefRows - 1) * intParam.MaxCUSize; // -1 row due to deblocking lag
        // New thread model sets up post proc (deblock) as ref task dependency; no deblocking lag, encode is leading.
        intParam.m_lagBehindRefRows = 2;
        intParam.m_meSearchRangeY = intParam.m_lagBehindRefRows * intParam.MaxCUSize;
        if (optHevc.EnableCm == ON) {
            intParam.m_lagBehindRefRows = (hevcParam.PicHeightInLumaSamples + maxCUSize - 1) / maxCUSize + 1; // for GACC - PicHeightInCtbs+1
            intParam.m_meSearchRangeY = intParam.m_lagBehindRefRows * maxCUSize;
        }

        intParam.AddCUDepth  = 0;
        while ((intParam.MaxCUSize >> intParam.MaxCUDepth) > (1u << (intParam.QuadtreeTULog2MinSize + intParam.AddCUDepth)))
            intParam.AddCUDepth++;

        intParam.MaxCUDepth += intParam.AddCUDepth;
        intParam.AddCUDepth++;

        intParam.MinCUSize = intParam.MaxCUSize >> (intParam.MaxCUDepth - intParam.AddCUDepth);
        intParam.MinTUSize = intParam.MaxCUSize >> intParam.MaxCUDepth;

        intParam.CropLeft = fi.CropX;
        intParam.CropTop = fi.CropY;
        intParam.CropRight = hevcParam.PicWidthInLumaSamples - fi.CropW - fi.CropX;
        intParam.CropBottom = hevcParam.PicHeightInLumaSamples - fi.CropH - fi.CropY;
        intParam.Width = hevcParam.PicWidthInLumaSamples;
        intParam.Height = hevcParam.PicHeightInLumaSamples;
        intParam.Log2NumPartInCU = intParam.MaxCUDepth << 1;
        intParam.NumPartInCU = 1 << intParam.Log2NumPartInCU;
        intParam.NumPartInCUSize  = 1 << intParam.MaxCUDepth;
        intParam.PicWidthInMinCbs = intParam.Width / intParam.MinCUSize;
        intParam.PicHeightInMinCbs = intParam.Height / intParam.MinCUSize;
        intParam.PicWidthInCtbs = (intParam.Width + intParam.MaxCUSize - 1) / intParam.MaxCUSize;
        intParam.PicHeightInCtbs = (intParam.Height + intParam.MaxCUSize - 1) / intParam.MaxCUSize;

        intParam.NumSlices = mfx.NumSlice;
        intParam.NumTileCols = tiles.NumTileColumns;
        intParam.NumTileRows = tiles.NumTileRows;
        intParam.NumTiles = tiles.NumTileColumns * tiles.NumTileRows;
        intParam.RegionIdP1 = (region.RegionEncoding == MFX_HEVC_REGION_ENCODING_ON && region.RegionType == MFX_HEVC_REGION_SLICE && intParam.NumSlices > 1) ? region.RegionId + 1 : 0;
        intParam.WPPFlag = (optHevc.WPP == ON);

        intParam.num_thread_structs = (intParam.WPPFlag) ? intParam.num_threads : MIN((Ipp32u)MAX(intParam.NumSlices, intParam.NumTiles) * 2 * intParam.m_framesInParallel, intParam.num_threads);
        intParam.num_bs_subsets = (intParam.WPPFlag) ? intParam.PicHeightInCtbs : MAX(intParam.NumSlices, intParam.NumTiles);

        for (Ipp32s i = 0; i < intParam.MaxCUDepth; i++ )
            intParam.AMPAcc[i] = (i < intParam.MaxCUDepth - intParam.AddCUDepth) ? (intParam.partModes == 3) : 0;

        // deltaQP control
        intParam.MaxCuDQPDepth = 0;
        intParam.m_maxDeltaQP = 0;
        intParam.UseDQP = 0;

        intParam.numRoi = roi.NumROI;

        if (optHevc.EnableCm == ON)
            intParam.numRoi = 0;

        if (intParam.numRoi > 0) {
            Ipp32s maxAbsDeltaQp = -1;
            if (mfx.RateControlMethod == CBR)
                maxAbsDeltaQp = 1;
            else if (mfx.RateControlMethod == VBR)
                maxAbsDeltaQp = 2;
            else if (mfx.RateControlMethod != CQP)
                maxAbsDeltaQp = 3;
            for (Ipp32s i = 0; i < intParam.numRoi; i++) {
                intParam.roi[i].left = roi.ROI[i].Left;
                intParam.roi[i].top = roi.ROI[i].Top;
                intParam.roi[i].right = roi.ROI[i].Right;
                intParam.roi[i].bottom = roi.ROI[i].Bottom;
                intParam.roi[i].priority = roi.ROI[i].Priority;
                if (maxAbsDeltaQp > 0)
                    intParam.roi[i].priority = Saturate(-maxAbsDeltaQp, maxAbsDeltaQp, intParam.roi[i].priority);
            }
            intParam.DeltaQpMode = 0;
        }

        if (intParam.DeltaQpMode) {
            intParam.MaxCuDQPDepth = 0;
            intParam.m_maxDeltaQP = 0;
            intParam.UseDQP = 1;
        }
        if (intParam.MaxCuDQPDepth > 0 || intParam.m_maxDeltaQP > 0 || intParam.numRoi > 0)
            intParam.UseDQP = 1;

        if (intParam.UseDQP)
            intParam.MinCuDQPSize = intParam.MaxCUSize >> intParam.MaxCuDQPDepth;
        else
            intParam.MinCuDQPSize = intParam.MaxCUSize;

        intParam.NumMinTUInMaxCU = intParam.MaxCUSize >> intParam.QuadtreeTULog2MinSize;

        intParam.writeAud = (opt.AUDelimiter == ON);

        intParam.FrameRateExtN = fi.FrameRateExtN;
        intParam.FrameRateExtD = fi.FrameRateExtD;
        intParam.vuiParametersPresentFlag = (opt2.DisableVUI == OFF);
        intParam.hrdPresentFlag = (opt2.DisableVUI == OFF && (mfx.RateControlMethod == CBR || mfx.RateControlMethod == VBR));
        intParam.cbrFlag = (mfx.RateControlMethod == CBR);

        if (mfx.RateControlMethod != CQP) {
            intParam.targetBitrate = MIN(0xfffffed8, (Ipp64u)mfx.TargetKbps * mfx.BRCParamMultiplier * 1000);
            if (intParam.hrdPresentFlag) {
                intParam.cpbSize = MIN(0xffffE380, (Ipp64u)mfx.BufferSizeInKB * mfx.BRCParamMultiplier * 8000);
                intParam.initDelay = MIN(0xffffE380, (Ipp64u)mfx.InitialDelayInKB * mfx.BRCParamMultiplier * 8000);
                intParam.hrdBitrate = intParam.cbrFlag ? intParam.targetBitrate : MIN(0xfffffed8, (Ipp64u)mfx.MaxKbps * mfx.BRCParamMultiplier * 1000);
            }
        }

        intParam.AspectRatioW  = fi.AspectRatioW;
        intParam.AspectRatioH  = fi.AspectRatioH;

        intParam.Profile = mfx.CodecProfile;
        intParam.Tier = (mfx.CodecLevel & MFX_TIER_HEVC_HIGH) ? 1 : 0;
        intParam.Level = (mfx.CodecLevel &~ MFX_TIER_HEVC_HIGH) * 1; // mult 3 it SetProfileLevel
        intParam.generalConstraintFlags = hevcParam.GeneralConstraintFlags;
        intParam.transquantBypassEnableFlag = 0;
        intParam.transformSkipEnabledFlag = 0;
        intParam.log2ParallelMergeLevel = 2;
        intParam.weightedPredFlag = 0;
        intParam.weightedBipredFlag = 0;
        intParam.constrainedIntrapredFlag = 0;
        intParam.strongIntraSmoothingEnabledFlag = 1;

        intParam.tcDuration90KHz = (mfxF64)fi.FrameRateExtD / fi.FrameRateExtN * 90000; // calculate tick duration
        intParam.tileColWidthMax = intParam.tileRowHeightMax = 0;

        Ipp16u tileColStart = 0;
        Ipp16u tileRowStart = 0;
        for (Ipp32s i = 0; i < intParam.NumTileCols; i++) {
            intParam.tileColWidth[i] = ((i + 1) * intParam.PicWidthInCtbs) / intParam.NumTileCols -
                (i * intParam.PicWidthInCtbs / intParam.NumTileCols);
            if (intParam.tileColWidthMax < intParam.tileColWidth[i])
                intParam.tileColWidthMax = intParam.tileColWidth[i];
            intParam.tileColStart[i] = tileColStart;
            tileColStart += intParam.tileColWidth[i];
        }
        for (Ipp32s i = 0; i < intParam.NumTileRows; i++) {
            intParam.tileRowHeight[i] = ((i + 1) * intParam.PicHeightInCtbs) / intParam.NumTileRows -
                (i * intParam.PicHeightInCtbs / intParam.NumTileRows);
            if (intParam.tileRowHeightMax < intParam.tileRowHeight[i])
                intParam.tileRowHeightMax = intParam.tileRowHeight[i];
            intParam.tileRowStart[i] = tileRowStart;
            tileRowStart += intParam.tileRowHeight[i];
        }

        intParam.doDumpRecon = (dumpFiles.ReconFilename[0] != 0);
        if (intParam.doDumpRecon)
            Copy(intParam.reconDumpFileName, dumpFiles.ReconFilename);

        intParam.doDumpSource = (dumpFiles.InputFramesFilename[0] != 0);
        if (intParam.doDumpSource)
            Copy(intParam.sourceDumpFileName, dumpFiles.InputFramesFilename);

        intParam.inputVideoMem = (mfxParam.IOPattern == VIDMEM) || (mfxParam.IOPattern == OPAQMEM && (opaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) == 0);

        intParam.randomRepackThreshold = Ipp32s(optHevc.RepackProb / 100.0 * MYRAND_MAX);
    }


    Ipp8s GetConstQp(const Frame &frame, const H265VideoParam &param, Ipp32s repackCounter)
    {
        Ipp8s sliceQpY;
        switch (frame.m_picCodeType)
        {
        case MFX_FRAMETYPE_I:
            sliceQpY = param.QPI;
            break;
        case MFX_FRAMETYPE_P:
            if (param.BiPyramidLayers > 1 && param.longGop && frame.m_biFramesInMiniGop == 15)
                sliceQpY = param.QPI;
            else if (param.PGopPicSize > 1)
                sliceQpY = param.QPP + frame.m_pyramidLayer;
            else
                sliceQpY = param.QPP;
            break;
        case MFX_FRAMETYPE_B:
            if (param.BiPyramidLayers > 1 && param.longGop && frame.m_biFramesInMiniGop == 15)
                sliceQpY = param.QPI + frame.m_pyramidLayer;
            else if (param.BiPyramidLayers > 1)
                sliceQpY = param.QPB + frame.m_pyramidLayer;
            else
                sliceQpY = param.QPB;
            break;
        default:
            // Unsupported Picture Type
            assert(0);
            sliceQpY = param.QPI;
            break;
        }

        sliceQpY += repackCounter;
        if (sliceQpY > 51)
            sliceQpY = 51;
        return sliceQpY;
    }


    Ipp8s GetRateQp(const Frame &in, const H265VideoParam &param, BrcIface *brc)
    {
        if (brc == NULL)
            return 51;

        Frame *frames[128] = {(Frame*)&in};
        Ipp32s framesCount = MIN(127, (Ipp32s)in.m_futureFrames.size());

        for (Ipp32s frmIdx = 0; frmIdx < framesCount; frmIdx++)
            frames[frmIdx+1] = in.m_futureFrames[frmIdx];

        return brc->GetQP(const_cast<H265VideoParam *>(&param), frames, framesCount+1);
    }


    void SetFrameDataAllocInfo(FrameData::AllocInfo &allocInfo, Ipp32s widthLu, Ipp32s heightLu, Ipp32s paddingLu, Ipp32s fourcc)
    {
        Ipp32s bpp;
        Ipp32s heightCh;

        switch (fourcc) {
        case MFX_FOURCC_NV12:
            allocInfo.bitDepthLu = 8;
            allocInfo.bitDepthCh = 8;
            allocInfo.chromaFormat = MFX_CHROMAFORMAT_YUV420;
            allocInfo.paddingChH = paddingLu/2;
            heightCh = heightLu/2;
            bpp = 1;
            break;
        case MFX_FOURCC_NV16:
            allocInfo.bitDepthLu = 8;
            allocInfo.bitDepthCh = 8;
            allocInfo.chromaFormat = MFX_CHROMAFORMAT_YUV422;
            allocInfo.paddingChH = paddingLu;
            heightCh = heightLu;
            bpp = 1;
            break;
        case MFX_FOURCC_P010:
            allocInfo.bitDepthLu = 10;
            allocInfo.bitDepthCh = 10;
            allocInfo.chromaFormat = MFX_CHROMAFORMAT_YUV420;
            allocInfo.paddingChH = paddingLu/2;
            heightCh = heightLu/2;
            bpp = 2;
            break;
        case MFX_FOURCC_P210:
            allocInfo.bitDepthLu = 10;
            allocInfo.bitDepthCh = 10;
            allocInfo.chromaFormat = MFX_CHROMAFORMAT_YUV422;
            allocInfo.paddingChH = paddingLu;
            heightCh = heightLu;
            bpp = 2;
            break;
        default:
            assert(!"invalid fourcc");
            return;
        }

        Ipp32s widthCh = widthLu;
        allocInfo.width = widthLu;
        allocInfo.height = heightLu;
        allocInfo.paddingLu = paddingLu;
        allocInfo.paddingChW = paddingLu;
        allocInfo.alignment = 32;

        allocInfo.pitchInBytesLu = (widthLu + paddingLu * 2) * bpp;
        allocInfo.sizeInBytesLu = allocInfo.pitchInBytesLu * (heightLu + paddingLu * 2);
        allocInfo.pitchInBytesCh = (widthCh + allocInfo.paddingChW * 2) * bpp;
        allocInfo.sizeInBytesCh = allocInfo.pitchInBytesCh * (heightCh + allocInfo.paddingChH * 2);
        allocInfo.feiHdl = NULL;
    }


    struct Deleter { template <typename T> void operator ()(T* p) const { delete p; } };

    Ipp32s blkSizeInternal2Fei[4] = { MFX_FEI_H265_BLK_8x8, MFX_FEI_H265_BLK_16x16, MFX_FEI_H265_BLK_32x32, MFX_FEI_H265_BLK_64x64 };
    Ipp32s blkSizeFei2Internal[12] = { -1, -1, -1, 2, -1, -1, 1, -1, -1, 0, -1, -1 };

}; // anonimous namespace


namespace H265Enc {
    struct H265EncodeTaskInputParams
    {
        mfxEncodeCtrl    *ctrl;
        mfxFrameSurface1 *surface;
        mfxBitstream     *bs;
        Frame            *m_targetFrame;  // this frame has to be encoded completely. it is our expected output.
        volatile Ipp32u   m_doStage;
        volatile Ipp32u   m_threadCount;
        volatile Ipp32u   m_reencode;     // BRC repack
    };
};


H265Encoder::H265Encoder(MFXCoreInterface1 &core)
    : m_core(core)
    , m_memBuf(NULL)
    , m_brc(NULL)
    , m_fei(NULL)
{
    m_videoParam.m_logMvCostTable = NULL;
    ippStaticInit();
    vm_cond_set_invalid(&m_condVar);
    vm_mutex_set_invalid(&m_critSect);
    vm_cond_set_invalid(&m_feiCondVar);
    vm_mutex_set_invalid(&m_feiCritSect);
    Zero(m_stat);
}


H265Encoder::~H265Encoder()
{
    vm_mutex_lock(&m_feiCritSect);
    m_pauseFeiThread = 0;
    m_stopFeiThread = 1;
    vm_mutex_unlock(&m_feiCritSect);
    vm_cond_signal(&m_feiCondVar);
    m_feiThread.Close();

    for_each(m_free.begin(), m_free.end(), Deleter());
    m_free.resize(0);
    for_each(m_inputQueue.begin(), m_inputQueue.end(), Deleter());
    m_inputQueue.resize(0);
    for_each(m_lookaheadQueue.begin(), m_lookaheadQueue.end(), Deleter());
    m_lookaheadQueue.resize(0);
    for_each(m_dpb.begin(), m_dpb.end(), Deleter());
    m_dpb.resize(0);
    // note: m_encodeQueue() & m_outputQueue() only "refer on tasks", not have real ptr. so we don't need to release ones

    delete m_brc;

    if (m_videoParam.m_logMvCostTable)
        H265_Free(m_videoParam.m_logMvCostTable);

    if (!m_frameEncoder.empty()) {
        for (size_t encIdx = 0; encIdx < m_frameEncoder.size(); encIdx++) {
            if (H265FrameEncoder* fenc = m_frameEncoder[encIdx]) {
                fenc->Close();
                delete fenc;
            }
        }
        m_frameEncoder.resize(0);
    }

    m_inputFrameDataPool.Destroy();
    m_reconFrameDataPool.Destroy();
    m_feiInputDataPool.Destroy();
    m_feiReconDataPool.Destroy();

    // destory FEI resources
    for (Ipp32s i = 0; i < 4; i++)
        m_feiAngModesPool[i].Destroy();
    for (Ipp32s i = 0; i < 4; i++)
        m_feiInterDataPool[i].Destroy();
    m_feiCuDataPool.Destroy();
    m_feiSaoModesPool.Destroy();
    if (m_fei)
        H265FEI_Close(m_fei);

    m_la.reset(0);

    vm_cond_destroy(&m_condVar);
    vm_mutex_destroy(&m_critSect);

    vm_cond_destroy(&m_feiCondVar);
    vm_mutex_destroy(&m_feiCritSect);

    if (m_memBuf) {
        H265_Free(m_memBuf);
        m_memBuf = NULL;
    }
}

namespace {
    mfxHDL CreateAccelerationDevice(MFXCoreInterface1 &core, mfxHandleType &type)
    {
        mfxCoreParam par;
        mfxStatus sts = MFX_ERR_NONE;
        if ((sts = core.GetCoreParam(&par)) != MFX_ERR_NONE)
            return mfxHDL(0);
        mfxU32 impl = par.Impl & 0x0700;

        mfxHDL device = mfxHDL(0);
        mfxHandleType handleType = (impl == MFX_IMPL_VIA_D3D9)  ? MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 :
                                   (impl == MFX_IMPL_VIA_D3D11) ? MFX_HANDLE_D3D11_DEVICE :
                                   (impl == MFX_IMPL_VIA_VAAPI) ? MFX_HANDLE_VA_DISPLAY :
                                                                  mfxHandleType(0);
        if (handleType == mfxHandleType(0))
            return mfxHDL(0);
        if ((sts = core.CreateAccelerationDevice(handleType, &device)) != MFX_ERR_NONE)
            return mfxHDL(0);
        type = handleType;
        return device;
    }
};

mfxStatus H265Encoder::Init(const mfxVideoParam &par)
{
    ConvertToInternalParam(m_videoParam, par);

    mfxStatus sts = MFX_ERR_NONE;

    mfxHDL device;
    mfxHandleType deviceType = mfxHandleType(0);
    if (m_videoParam.enableCmFlag) {
        if ((device = CreateAccelerationDevice(m_core, deviceType)) == 0)
            return MFX_ERR_DEVICE_FAILED;
    }

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        m_brc = CreateBrc(&par);
        if ((sts = m_brc->Init(&par, m_videoParam)) != MFX_ERR_NONE)
            return sts;
    }

    if ((sts = InitInternal()) != MFX_ERR_NONE)
        return sts;

    SetProfileLevel();
    SetVPS();
    SetSPS();
    SetPPS();
    SetSeiAps();

    Ipp8u tmpbuf[1024];
    H265BsReal bs;

    bs.m_base.m_pbsBase = tmpbuf;
    bs.m_base.m_maxBsSize = sizeof(tmpbuf);
    bs.Reset();
    PutVPS(&bs, m_vps, m_profile_level);
    bs.WriteTrailingBits();
    m_vpsBufSize = bs.WriteNAL(m_vpsBuf, sizeof(m_vpsBuf), 0, NAL_VPS);

    bs.m_base.m_pbsBase = tmpbuf;
    bs.m_base.m_maxBsSize = sizeof(tmpbuf);
    bs.Reset();
    PutSPS(&bs, m_sps, m_profile_level);
    bs.WriteTrailingBits();
    m_spsBufSize = bs.WriteNAL(m_spsBuf, sizeof(m_spsBuf), 0, NAL_SPS);

    bs.m_base.m_pbsBase = tmpbuf;
    bs.m_base.m_maxBsSize = sizeof(tmpbuf);
    bs.Reset();
    PutPPS(&bs, m_pps);
    bs.WriteTrailingBits();
    m_ppsBufSize = bs.WriteNAL(m_ppsBuf, sizeof(m_ppsBuf), 0, NAL_PPS);

    if (m_videoParam.hrdPresentFlag)
        m_hrd.Init(m_sps, m_videoParam.initDelay);

    // memsize calculation
    Ipp32s sizeofH265CU = (m_videoParam.bitDepthLuma > 8 ? sizeof(H265CU<Ipp16u>) : sizeof(H265CU<Ipp8u>));
    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    Ipp32s memSize = 0;
    // cu
    memSize = sizeofH265CU * m_videoParam.num_thread_structs + DATA_ALIGN;
    // m_tile_id
    memSize += numCtbs * sizeof(Ipp16u);
    memSize += m_videoParam.NumTiles * sizeof(Segment) + DATA_ALIGN;
    // m_slice_id
    memSize += numCtbs * sizeof(Ipp16u);
    memSize += m_videoParam.NumSlices * sizeof(Segment) + DATA_ALIGN;
    // m_bsf
    memSize += sizeof(H265BsFake)*m_videoParam.num_thread_structs + DATA_ALIGN;
    // data_temp
    Ipp32s data_temp_size = ((MAX_TOTAL_DEPTH * 2 + 2) << m_videoParam.Log2NumPartInCU);
    memSize += sizeof(H265CUData) * data_temp_size * m_videoParam.num_thread_structs + DATA_ALIGN;//for ModeDecision try different cu
    // coeffs Work
    //memSize += sizeof(CoeffsType) * (numCtbs << (m_videoParam.Log2MaxCUSize << 1)) * 6 / (2 + m_videoParam.chromaShift) + DATA_ALIGN;

    // allocation
    m_memBuf = (Ipp8u *)H265_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(m_memBuf);
    ippsZero_8u(m_memBuf, memSize);

    // mapping
    Ipp8u *ptr = m_memBuf;
    m_cu = UMC::align_pointer<void*>(ptr, DATA_ALIGN);
    ptr += sizeofH265CU * m_videoParam.num_thread_structs + DATA_ALIGN;
    //m_cu = UMC::align_pointer<void*>(ptr, 0x1000);
    //ptr += sizeofH265CU * m_videoParam.num_thread_structs + 0x1000;

    // m_tile_id
    m_tile_ids = (Ipp16u*)ptr;
    ptr += numCtbs * sizeof(Ipp16u);
    m_tiles = UMC::align_pointer<Segment *>(ptr, DATA_ALIGN);
    ptr += m_videoParam.NumTiles * sizeof(Segment) + DATA_ALIGN;
    // m_slice
    m_slice_ids = (Ipp16u*)ptr;
    ptr += numCtbs * sizeof(Ipp16u);
    m_slices = UMC::align_pointer<Segment *>(ptr, DATA_ALIGN);
    ptr += m_videoParam.NumSlices * sizeof(Segment) + DATA_ALIGN;
    // m_bsf
    m_bsf = UMC::align_pointer<H265BsFake *>(ptr, DATA_ALIGN);
    ptr += sizeof(H265BsFake)*(m_videoParam.num_thread_structs) + DATA_ALIGN;
    // data_temp
    data_temp = UMC::align_pointer<H265CUData *>(ptr, DATA_ALIGN);
    ptr += sizeof(H265CUData) * data_temp_size * m_videoParam.num_thread_structs + DATA_ALIGN;


    // ConfigureTileFragmentation()
    m_videoParam.m_tile_ids = m_tile_ids;
    m_videoParam.m_tiles = m_tiles;
    for (Ipp32s curr_tile = 0; curr_tile < m_videoParam.NumTiles; curr_tile++) {
        Ipp32u ctb_addr = m_videoParam.tileRowStart[curr_tile / m_videoParam.NumTileCols] * m_videoParam.PicWidthInCtbs + m_videoParam.tileColStart[curr_tile % m_videoParam.NumTileCols];
        m_videoParam.m_tiles[curr_tile].first_ctb_addr = ctb_addr;
        for (Ipp32u j = 0; j < m_videoParam.tileRowHeight[curr_tile / m_videoParam.NumTileCols]; j++) {
            for (Ipp32u i = 0; i < m_videoParam.tileColWidth[curr_tile % m_videoParam.NumTileCols]; i++)
                m_tile_ids[ctb_addr++] = curr_tile;
            ctb_addr += m_videoParam.PicWidthInCtbs - m_videoParam.tileColWidth[curr_tile % m_videoParam.NumTileCols];
        }
        m_videoParam.m_tiles[curr_tile].last_ctb_addr = ctb_addr - m_videoParam.PicWidthInCtbs + m_videoParam.tileColWidth[curr_tile % m_videoParam.NumTileCols] - 1;
    }


    // ConfigureSliceFragmentation()
    m_videoParam.m_slice_ids = m_slice_ids;
    m_videoParam.m_slices = m_slices;
    Ipp32s sliceRowStart = 0;
    for (Ipp32s i = 0; i < m_videoParam.NumSlices; i++) {
        Ipp32s sliceHeight = ((i + 1) * m_videoParam.PicHeightInCtbs) / m_videoParam.NumSlices -
            (i * m_videoParam.PicHeightInCtbs / m_videoParam.NumSlices);
        Ipp32u firstAddr = m_videoParam.m_slices[i].first_ctb_addr = sliceRowStart * m_videoParam.PicWidthInCtbs;
        Ipp32u lastAddr = m_videoParam.m_slices[i].last_ctb_addr = (sliceRowStart + sliceHeight) * m_videoParam.PicWidthInCtbs - 1;

        for (Ipp32u addr = firstAddr; addr <= lastAddr; addr++)
            m_slice_ids[addr] = i;

        sliceRowStart += sliceHeight;
    }

    // AllocFrameEncoders()
    m_frameEncoder.resize(m_videoParam.m_framesInParallel);
    for (size_t encIdx = 0; encIdx < m_frameEncoder.size(); encIdx++) {
        m_frameEncoder[encIdx] = new H265FrameEncoder(*this);
        mfxStatus st = m_frameEncoder[encIdx]->Init();
        if (st != MFX_ERR_NONE)
            return st;
    }

    if (vm_cond_init(&m_condVar) != VM_OK)
        return MFX_ERR_MEMORY_ALLOC;
    if (vm_mutex_init(&m_critSect) != VM_OK)
        return MFX_ERR_MEMORY_ALLOC;

    if (m_videoParam.enableCmFlag) {
        if (vm_cond_init(&m_feiCondVar) != VM_OK)
            return MFX_ERR_MEMORY_ALLOC;
        if (vm_mutex_init(&m_feiCritSect) != VM_OK)
            return MFX_ERR_MEMORY_ALLOC;
    }

    if (m_videoParam.SceneCut || m_videoParam.DeltaQpMode || m_videoParam.AnalyzeCmplx)
        m_la.reset(new Lookahead(*this));

    FrameData::AllocInfo frameDataAllocInfo;
    SetFrameDataAllocInfo(frameDataAllocInfo, m_videoParam.Width, m_videoParam.Height, 96, m_videoParam.fourcc);

    if (m_videoParam.enableCmFlag) {
        mfxFEIH265Param feiParams = {};
        feiParams.Width = m_videoParam.Width;
        feiParams.Height = m_videoParam.Height;
        feiParams.Padding = frameDataAllocInfo.paddingLu;
        feiParams.WidthChroma = m_videoParam.Width;
        feiParams.HeightChroma = m_videoParam.Height >> m_videoParam.chromaShiftH;
        feiParams.PaddingChroma = frameDataAllocInfo.paddingChW;
        feiParams.MaxCUSize = m_videoParam.MaxCUSize;
        feiParams.MPMode = m_videoParam.partModes;
        feiParams.NumRefFrames = m_videoParam.MaxDecPicBuffering;
        feiParams.TargetUsage = 0;
        feiParams.NumIntraModes = 1;
        feiParams.FourCC = m_videoParam.fourcc;
        feiParams.EnableChromaSao = m_videoParam.SAOChromaFlag;

        mfxExtFEIH265Alloc feiAlloc = {};
        MFXCoreInterface core(m_core.m_core);
        mfxStatus sts = H265FEI_GetSurfaceDimensions_new(&core, &feiParams, &feiAlloc);
        if (sts != MFX_ERR_NONE)
            return sts;

        sts = H265FEI_Init(&m_fei, &feiParams, &core);
        if (sts != MFX_ERR_NONE)
            return sts;

        FeiOutData::AllocInfo feiOutAllocInfo;
        feiOutAllocInfo.feiHdl = m_fei;
        for (Ipp32s blksize = 0; blksize < 4; blksize++) {
            feiOutAllocInfo.allocInfo = feiAlloc.IntraMode[blksize];
            m_feiAngModesPool[blksize].Init(feiOutAllocInfo, 0);
        }

        for (Ipp32s blksize = 0; blksize < 4; blksize++) {
            Ipp32s feiBlkSizeIdx = blkSizeInternal2Fei[blksize];

            feiOutAllocInfo.allocInfo = feiAlloc.InterData[feiBlkSizeIdx];
            m_feiInterDataPool[blksize].Init(feiOutAllocInfo, 0);
        }

        FeiBufferUp::AllocInfo feiBufferUpAllocInfo;
        feiBufferUpAllocInfo.feiHdl = m_fei;
        feiBufferUpAllocInfo.size = numCtbs * sizeof(SaoOffsetOut_gfx) * 3; //(Y U V)
        feiBufferUpAllocInfo.alignment = 0x1000;
        m_feiSaoModesPool.Init(feiBufferUpAllocInfo, 0);

        // patch allocInfo to meet CmSurface2D requirements
        frameDataAllocInfo.feiHdl = m_fei;
        frameDataAllocInfo.alignment = 0x1000; // page aligned
        frameDataAllocInfo.sizeInBytesLu = frameDataAllocInfo.sizeInBytesLu / frameDataAllocInfo.pitchInBytesLu * feiAlloc.SrcRefLuma.pitch;
        frameDataAllocInfo.sizeInBytesCh = frameDataAllocInfo.sizeInBytesCh / frameDataAllocInfo.pitchInBytesCh * feiAlloc.SrcRefChroma.pitch;
        frameDataAllocInfo.sizeInBytesLu = IPP_MAX(frameDataAllocInfo.sizeInBytesLu, (Ipp32s)feiAlloc.SrcRefLuma.size);
        frameDataAllocInfo.sizeInBytesCh = IPP_MAX(frameDataAllocInfo.sizeInBytesCh, (Ipp32s)feiAlloc.SrcRefChroma.size);
        frameDataAllocInfo.pitchInBytesLu = feiAlloc.SrcRefLuma.pitch;
        frameDataAllocInfo.pitchInBytesCh = feiAlloc.SrcRefChroma.pitch;

        FeiInputData::AllocInfo feiInputAllocaInfo = {};
        feiInputAllocaInfo.feiHdl = m_fei;
        m_feiInputDataPool.Init(feiInputAllocaInfo, 0);

        FeiReconData::AllocInfo feiReconAllocaInfo = {};
        feiReconAllocaInfo.feiHdl = m_fei;
        m_feiReconDataPool.Init(feiReconAllocaInfo, 0);
    }

    FeiBufferUp::AllocInfo feiBufferUpAllocInfo;
    feiBufferUpAllocInfo.feiHdl = m_fei;
    feiBufferUpAllocInfo.alignment = m_videoParam.enableCmFlag ? 0x1000 : DATA_ALIGN;
    feiBufferUpAllocInfo.size = (numCtbs << m_videoParam.Log2NumPartInCU) * sizeof(H265CUData);
    m_feiCuDataPool.Init(feiBufferUpAllocInfo, 0);

    if (m_videoParam.inputVideoMem && m_videoParam.enableCmFlag) {
        m_cmcopy.reset(new CmCopy());
        if ((sts = m_cmcopy->Init(device, deviceType)) != MFX_ERR_NONE)
            return sts;
        sts = m_cmcopy->SetParam(frameDataAllocInfo.width, frameDataAllocInfo.height, m_videoParam.fourcc,
                                 frameDataAllocInfo.pitchInBytesLu, frameDataAllocInfo.pitchInBytesCh,
                                 frameDataAllocInfo.paddingLu, frameDataAllocInfo.paddingChW);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    m_reconFrameDataPool.Init(frameDataAllocInfo, 0);
    m_inputFrameDataPool.Init(frameDataAllocInfo, 0);

    if (m_la.get() && (m_videoParam.LowresFactor || m_videoParam.SceneCut)) {
        Ipp32s lowresFactor = m_videoParam.LowresFactor;
        if (m_videoParam.SceneCut && m_videoParam.LowresFactor == 0)
            lowresFactor = 1;
        SetFrameDataAllocInfo(frameDataAllocInfo, m_videoParam.Width>>lowresFactor, m_videoParam.Height>>lowresFactor,
                              MAX(SIZE_BLK_LA, 16) + 16, m_videoParam.fourcc);
        m_frameDataLowresPool.Init(frameDataAllocInfo, 0);
    }

    if (m_la.get()) {
        if (m_videoParam.LowresFactor) {
            Statistics::AllocInfo statsAllocInfo;
            statsAllocInfo.width = m_videoParam.Width >> m_videoParam.LowresFactor;
            statsAllocInfo.height = m_videoParam.Height >> m_videoParam.LowresFactor;
            m_statsLowresPool.Init(statsAllocInfo, 0);
        }
        if (m_videoParam.DeltaQpMode || m_videoParam.LowresFactor == 0) {
            Statistics::AllocInfo statsAllocInfo;
            statsAllocInfo.width = m_videoParam.Width;
            statsAllocInfo.height = m_videoParam.Height;
            m_statsPool.Init(statsAllocInfo, 0);
        }
    }

    m_stopFeiThread = 0;
    m_pauseFeiThread = 0;
    m_feiThreadRunning = 0;
    if (m_videoParam.enableCmFlag) {
        Ipp32s umcSts = m_feiThread.Create(H265Encoder::FeiThreadRoutineStarter, this);
        if (umcSts != UMC::UMC_OK)
            return MFX_ERR_MEMORY_ALLOC;
    }

    m_newFrame = NULL;
    m_laFrame = NULL;
    m_targetFrame = NULL;

    m_threadingTaskRunning = 0;
    m_frameCountSync = 0;
    m_frameCountBufferedSync = 0;

    m_ithreadPool.resize(m_videoParam.num_thread_structs, 0);
    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::InitInternal()
{
    Ipp32u memSize = 0;

    memSize += (1 << 16);
    m_videoParam.m_logMvCostTable = (Ipp8u *)H265_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(m_videoParam.m_logMvCostTable);

    // init lookup table for 2*log2(x)+2
    m_videoParam.m_logMvCostTable[(1 << 15)] = 1;
    m_videoParam.m_logMvCostTable[0] = 32;
    Ipp64f log2reciproc = 2 / log(2.0);
    for (Ipp32s i = 1; i < (1 << 15); i++) {
        m_videoParam.m_logMvCostTable[(1 << 15) + i] = m_videoParam.m_logMvCostTable[(1 << 15) - i] = (Ipp8u)(log(i + 1.0) * log2reciproc + 2);
    }

    m_profileIndex = 0;
    m_frameOrder = 0;

    m_frameOrderOfLastIdr = 0;              // frame order of last IDR frame (in display order)
    m_frameOrderOfLastIntra = 0;            // frame order of last I-frame (in display order)
    m_frameOrderOfLastIntraInEncOrder = 0;  // frame order of last I-frame (in encoding order)
    m_frameOrderOfLastAnchor = 0;           // frame order of last anchor (first in minigop) frame (in display order)
    m_frameOrderOfLastIdrB = 0;
    m_frameOrderOfLastIntraB = 0;
    m_frameOrderOfLastAnchorB  = 0;
    m_LastbiFramesInMiniGop  = 0;
    m_miniGopCount = -1;
    m_lastTimeStamp = (mfxU64)MFX_TIMESTAMP_UNKNOWN;
    m_lastEncOrder = -1;

    Zero(m_videoParam.cu_split_threshold_cu_sentinel);
    Zero(m_videoParam.cu_split_threshold_tu_sentinel);
    Zero(m_videoParam.cu_split_threshold_cu);
    Zero(m_videoParam.cu_split_threshold_tu);

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

//mfxStatus H265Encoder::CreateCmDevice()
//{
//    MFXCoreInterface core(m_core.m_core);
//    CmDevice *m_cmDevice = TryCreateCmDevicePtr(&core);
//    if (m_cmDevice == 0)
//        return MFX_ERR_DEVICE_FAILED;
//    return MFX_ERR_NONE;
//}

mfxStatus H265Encoder::Reset(const mfxVideoParam &par)
{
    return MFX_ERR_UNSUPPORTED;
}


void H265Encoder::GetEncodeStat(mfxEncodeStat &stat)
{
    MfxAutoMutex guard(m_statMutex);
    Copy(stat, m_stat);
}


// --------------------------------------------------------
//   Utils
// --------------------------------------------------------

void ReorderBiFrames(FrameIter inBegin, FrameIter inEnd, FrameList &in, FrameList &out, Ipp32s layer = 1)
{
    if (inBegin == inEnd)
        return;
    FrameIter pivot = inBegin;
    std::advance(pivot, (std::distance(inBegin, inEnd) - 1) / 2);
    (*pivot)->m_pyramidLayer = layer;
    FrameIter rightHalf = pivot;
    ++rightHalf;
    if (inBegin == pivot)
        ++inBegin;
    out.splice(out.end(), in, pivot);
    ReorderBiFrames(inBegin, rightHalf, in, out, layer + 1);
    ReorderBiFrames(rightHalf, inEnd, in, out, layer + 1);
}


void ReorderBiFramesLongGop(FrameIter inBegin, FrameIter inEnd, FrameList &in, FrameList &out)
{
    VM_ASSERT(std::distance(inBegin, inEnd) == 15);

    // 3 anchors + 3 mini-pyramids
    for (Ipp32s i = 0; i < 3; i++) {
        FrameIter anchor = inBegin;
        std::advance(anchor, 3);
        FrameIter afterAnchor = anchor;
        ++afterAnchor;
        (*anchor)->m_pyramidLayer = 2;
        out.splice(out.end(), in, anchor);
        ReorderBiFrames(inBegin, afterAnchor, in, out, 3);
        inBegin = afterAnchor;
    }
    // last 4th mini-pyramid
    ReorderBiFrames(inBegin, inEnd, in, out, 3);
}

void ReorderFrames(FrameList &input, FrameList &reordered, const H265VideoParam &param, Ipp32s endOfStream)
{
    Ipp32s closedGop = param.GopClosedFlag;
    Ipp32s strictGop = param.GopStrictFlag;
    Ipp32s biPyramid = param.BiPyramidLayers > 1;

    if (input.empty())
        return;

    FrameIter anchor = input.begin();
    FrameIter end = input.end();
    while (anchor != end && (*anchor)->m_picCodeType == MFX_FRAMETYPE_B)
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
            (*--anchor)->m_picCodeType = MFX_FRAMETYPE_P; // (b) use last B frame of stream as anchor
    }
    else {
        Frame *anchorFrm = (*anchor);
        if (anchorFrm->m_picCodeType == MFX_FRAMETYPE_I && (anchorFrm->m_isIdrPic || closedGop)) {
            if (strictGop)
                anchorExists = false; // (c) encode B frames without anchor
            else
                (*--anchor)->m_picCodeType = MFX_FRAMETYPE_P; // (d) use last B frame of current sequence as anchor
        }
    }

    // setup number of B frames
    Ipp32s numBiFrames = (Ipp32s)std::distance(input.begin(), anchor);
    for (FrameIter i = input.begin(); i != anchor; ++i)
        (*i)->m_biFramesInMiniGop = numBiFrames;

    // reorder anchor frame
    FrameIter afterBiFrames = anchor;
    if (anchorExists) {
        (*anchor)->m_pyramidLayer = 0; // anchor frames are from layer=0
        (*anchor)->m_biFramesInMiniGop = numBiFrames;
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


Frame* FindOldestOutputTask(FrameList & encodeQueue)
{
    FrameIter begin = encodeQueue.begin();
    FrameIter end = encodeQueue.end();

    if (begin == end)
        return NULL;

    FrameIter oldest = begin;
    for (++begin; begin != end; ++begin) {
        if ( (*oldest)->m_encOrder > (*begin)->m_encOrder)
            oldest = begin;
    }

    return *oldest;

} //

// --------------------------------------------------------
//              H265Encoder
// --------------------------------------------------------

Frame *H265Encoder::AcceptFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl *ctrl, mfxBitstream *mfxBS)
{
    Frame *inputFrame = NULL;

    if (surface) {
        if (m_free.empty()) {
            Frame *newFrame = new Frame;
            newFrame->Create(&m_videoParam);
            m_free.push_back(newFrame);
        }
        m_free.front()->ResetEncInfo();
        m_free.front()->m_timeStamp = surface->Data.TimeStamp;
        m_inputQueue.splice(m_inputQueue.end(), m_free, m_free.begin());
        inputFrame = m_inputQueue.back();
        inputFrame->AddRef();

        if (ctrl && ctrl->Payload && ctrl->NumPayload > 0) {
            inputFrame->m_userSeiMessages = ctrl->Payload;
            inputFrame->m_numUserSeiMessages = ctrl->NumPayload;
        }

        if (m_videoParam.encodedOrder) {
            ThrowIf(!inputFrame, std::runtime_error(""));
            ThrowIf(!ctrl, std::runtime_error(""));

            inputFrame->m_picCodeType = ctrl->FrameType;

            if (m_brc && m_brc->IsVMEBRC()) {
                const mfxExtLAFrameStatistics *vmeData = (mfxExtLAFrameStatistics *)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam,MFX_EXTBUFF_LOOKAHEAD_STAT);
                ThrowIf(!vmeData, std::runtime_error(""));
                mfxStatus sts = m_brc->SetFrameVMEData(vmeData, m_videoParam.Width, m_videoParam.Height);
                ThrowIf(sts != MFX_ERR_NONE, std::runtime_error(""));
                mfxLAFrameInfo *pInfo = &vmeData->FrameStat[0];
                inputFrame->m_picCodeType = pInfo->FrameType;
                inputFrame->m_frameOrder = pInfo->FrameDisplayOrder;
                inputFrame->m_pyramidLayer = pInfo->Layer;
                ThrowIf(inputFrame->m_pyramidLayer >= m_videoParam.BiPyramidLayers, std::runtime_error(""));
            }

            if (!(inputFrame->m_picCodeType & MFX_FRAMETYPE_B)) {
                m_frameOrderOfLastIdr = m_frameOrderOfLastIdrB;
                m_frameOrderOfLastIntra = m_frameOrderOfLastIntraB;
                m_frameOrderOfLastAnchor = m_frameOrderOfLastAnchorB;
            }
        }

        ConfigureInputFrame(inputFrame, !!m_videoParam.encodedOrder);
        UpdateGopCounters(inputFrame, !!m_videoParam.encodedOrder);
    } else {
        // tmp fix for short sequences
        if (m_la.get()) {
            if (m_videoParam.AnalyzeCmplx)
                for (std::list<Frame *>::iterator i = m_inputQueue.begin(); i != m_inputQueue.end(); ++i)
                    m_la->AverageComplexity(*i);
            m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue);
            m_la->ResetState();
        }
    }

    FrameList & inputQueue = m_la.get() ? m_lookaheadQueue : m_inputQueue;
    // reorder as many frames as possible
    while (!inputQueue.empty()) {
        size_t reorderedSize = m_reorderedQueue.size();

        if (m_videoParam.encodedOrder) {
            if (!inputQueue.empty())
                m_reorderedQueue.splice(m_reorderedQueue.end(), inputQueue, inputQueue.begin());
        } else {
            ReorderFrames(inputQueue, m_reorderedQueue, m_videoParam, surface == NULL);
        }

        if (reorderedSize == m_reorderedQueue.size())
            break; // nothing new, exit

        // configure newly reordered frames
        FrameIter i = m_reorderedQueue.begin();
        std::advance(i, reorderedSize);
        for (; i != m_reorderedQueue.end(); ++i) {
            ConfigureEncodeFrame(*i);
            m_lastEncOrder = (*i)->m_encOrder;
        }
    }

    while (!m_reorderedQueue.empty()
        && ((Ipp32s)m_reorderedQueue.size() >= m_videoParam.RateControlDepth - 1 || surface == NULL)
        && (Ipp32s)(m_encodeQueue.size() + m_outputQueue.size()) < m_videoParam.m_framesInParallel) {

        // assign resources for encoding
        m_reorderedQueue.front()->m_recon = m_reconFrameDataPool.Allocate();
        m_reorderedQueue.front()->m_feiRecon = m_feiReconDataPool.Allocate();
        m_reorderedQueue.front()->m_feiOrigin = m_feiInputDataPool.Allocate();
        m_reorderedQueue.front()->m_feiCuData = m_feiCuDataPool.Allocate();
        m_reorderedQueue.front()->cu_data = (H265CUData *)m_reorderedQueue.front()->m_feiCuData->m_sysmem;

        m_dpb.splice(m_dpb.end(), m_reorderedQueue, m_reorderedQueue.begin());
        m_encodeQueue.push_back(m_dpb.back());

        if (m_brc && m_videoParam.AnalyzeCmplx > 0) {
            Ipp32s laDepth = m_videoParam.RateControlDepth - 1;
            for (FrameIter i = m_reorderedQueue.begin(); i != m_reorderedQueue.end() && laDepth > 0; ++i, --laDepth)
                m_encodeQueue.back()->m_futureFrames.push_back(*i);
        }
    }

    return inputFrame;
}

void H265Encoder::EnqueueFrameEncoder(H265EncodeTaskInputParams *inputParam)
{
    Frame *frame = m_encodeQueue.front();

    if (frame->m_encIdx < 0) {
        for (size_t encIdx = 0; encIdx < m_frameEncoder.size(); encIdx++) {
            if (m_frameEncoder[encIdx]->IsFree()) {
                frame->m_encIdx = encIdx;
                m_frameEncoder[encIdx]->SetFree(false);
                break;
            }
        }
        ThrowIf(frame->m_encIdx < 0, std::runtime_error(""));
    }


    if (m_videoParam.enableCmFlag) {
        frame->m_ttSubmitGpuCopySrc.numDownstreamDependencies = 0;
        frame->m_ttSubmitGpuCopySrc.numUpstreamDependencies = 0;
        frame->m_ttSubmitGpuCopySrc.finished = 0;
        frame->m_ttSubmitGpuCopySrc.poc = frame->m_frameOrder;
        if (!frame->m_ttInitNewFrame.finished)
            AddTaskDependency(&frame->m_ttSubmitGpuCopySrc, &frame->m_ttInitNewFrame); // GPU_SUBMIT_COPY_SRC <- INIT_NEW_FRAME

        if (frame->m_slices[0].sliceIntraAngMode == INTRA_ANG_MODE_GRADIENT) {
            frame->m_ttSubmitGpuIntra.numDownstreamDependencies = 0;
            frame->m_ttSubmitGpuIntra.numUpstreamDependencies = 0;
            frame->m_ttSubmitGpuIntra.finished = 0;
            frame->m_ttSubmitGpuIntra.poc = frame->m_frameOrder;
            for (Ipp32s i = 0; i < 4; i++)
                if (!frame->m_feiIntraAngModes[i])
                   frame->m_feiIntraAngModes[i] = m_feiAngModesPool[i].Allocate();
            AddTaskDependency(&frame->m_ttSubmitGpuIntra, &frame->m_ttSubmitGpuCopySrc); // GPU_SUBMIT_INTRA <- GPU_SUBMIT_COPY_SRC

            frame->m_ttWaitGpuIntra.numDownstreamDependencies = 0;
            frame->m_ttWaitGpuIntra.numUpstreamDependencies = 0;
            frame->m_ttWaitGpuIntra.finished = 0;
            frame->m_ttWaitGpuIntra.poc = frame->m_frameOrder;
            frame->m_ttWaitGpuIntra.syncpoint = NULL;
            AddTaskDependency(&frame->m_ttWaitGpuIntra, &frame->m_ttSubmitGpuIntra); // GPU_WAIT_INTRA <- GPU_SUBMIT_INTRA
        }

        for (Ipp32s i = 0; i < 2; i++) {
            const RefPicList &list = frame->m_refPicList[i];
            for (Ipp32s j = 0; j < list.m_refFramesCount; j++) {
                if (i == 1 && frame->m_mapRefIdxL1ToL0[j] != -1)
                    continue;

                Frame *ref = list.m_refFrames[j];
                Ipp32s uniqRefIdx = frame->m_mapListRefUnique[i][j];

                for (Ipp32s blksize = 0; blksize < 4; blksize++) {
                    if (!frame->m_feiInterData[uniqRefIdx][blksize])
                        frame->m_feiInterData[uniqRefIdx][blksize] = m_feiInterDataPool[blksize].Allocate();
                }

                frame->m_ttSubmitGpuMe[uniqRefIdx].numDownstreamDependencies = 0;
                frame->m_ttSubmitGpuMe[uniqRefIdx].numUpstreamDependencies = 0;
                frame->m_ttSubmitGpuMe[uniqRefIdx].finished = 0;
                frame->m_ttSubmitGpuMe[uniqRefIdx].poc = frame->m_frameOrder;
                frame->m_ttSubmitGpuMe[uniqRefIdx].listIdx = i;
                frame->m_ttSubmitGpuMe[uniqRefIdx].refIdx = j;

                frame->m_ttWaitGpuMe[uniqRefIdx].numDownstreamDependencies = 0;
                frame->m_ttWaitGpuMe[uniqRefIdx].numUpstreamDependencies = 0;
                frame->m_ttWaitGpuMe[uniqRefIdx].finished = 0;
                frame->m_ttWaitGpuMe[uniqRefIdx].syncpoint = NULL;
                frame->m_ttWaitGpuMe[uniqRefIdx].poc = frame->m_frameOrder;

                AddTaskDependency(&frame->m_ttSubmitGpuMe[uniqRefIdx], &frame->m_ttSubmitGpuCopySrc); // GPU_SUBMIT_HME <- GPU_SUBMIT_COPY_SRC
                AddTaskDependency(&frame->m_ttWaitGpuMe[uniqRefIdx], &frame->m_ttSubmitGpuMe[uniqRefIdx]); // GPU_WAIT_ME16 <- GPU_SUBMIT_ME16
            }
        }

        if (frame->m_isRef || m_videoParam.enableCmPostProc && m_videoParam.doDumpRecon && frame->m_doPostProc) { // need for dump_rec
            frame->m_ttSubmitGpuCopyRec.numDownstreamDependencies = 0;
            frame->m_ttSubmitGpuCopyRec.numUpstreamDependencies = 0;
            frame->m_ttSubmitGpuCopyRec.finished = 0;
            frame->m_ttSubmitGpuCopyRec.poc = frame->m_frameOrder;

            frame->m_ttWaitGpuCopyRec.numDownstreamDependencies = 0;
            frame->m_ttWaitGpuCopyRec.numUpstreamDependencies = 0;
            frame->m_ttWaitGpuCopyRec.finished = 0;
            frame->m_ttWaitGpuCopyRec.poc = frame->m_frameOrder;
            frame->m_ttWaitGpuCopyRec.syncpoint = NULL;
            AddTaskDependency(&frame->m_ttWaitGpuCopyRec, &frame->m_ttSubmitGpuCopyRec); // GPU_WAIT_COPY_REC <- GPU_SUBMIT_COPY_REC

            if (frame->m_isRef && m_videoParam.enableCmPostProc && frame->m_doPostProc) {
                frame->m_ttPadRecon.numDownstreamDependencies = 0;
                frame->m_ttPadRecon.numUpstreamDependencies = 0;
                frame->m_ttPadRecon.finished = 0;
                frame->m_ttPadRecon.poc = frame->m_frameOrder;
                AddTaskDependency(&frame->m_ttPadRecon, &frame->m_ttWaitGpuCopyRec); // PAD_RECON <- GPU_WAIT_COPY_REC
            }

        }

        // GPU PostProc
        if (frame->m_doPostProc && m_videoParam.enableCmPostProc) {
            if (!frame->m_feiSaoModes)
                frame->m_feiSaoModes = m_feiSaoModesPool.Allocate();
            frame->m_ttSubmitGpuPostProc.numDownstreamDependencies = 0;
            frame->m_ttSubmitGpuPostProc.numUpstreamDependencies = 0;
            frame->m_ttSubmitGpuPostProc.finished = 0;
            frame->m_ttSubmitGpuPostProc.poc = frame->m_frameOrder;

            frame->m_ttWaitGpuPostProc.numDownstreamDependencies = 0;
            frame->m_ttWaitGpuPostProc.numUpstreamDependencies = 0;
            frame->m_ttWaitGpuPostProc.finished = 0;
            frame->m_ttWaitGpuPostProc.syncpoint = NULL;
            frame->m_ttWaitGpuPostProc.poc = frame->m_frameOrder;
            AddTaskDependency(&frame->m_ttWaitGpuPostProc, &frame->m_ttSubmitGpuPostProc); // GPU_WAIT_PP <- GPU_SUBMIT_PP

            if (frame->m_isRef || m_videoParam.doDumpRecon)
                AddTaskDependency(&frame->m_ttSubmitGpuCopyRec, &frame->m_ttSubmitGpuPostProc); // GPU_SUBMIT_COPY_REC <- GPU_SUBMIT_PP

            // adjust PP: replace complete postproc by deblock only
            {
                bool doSao = m_sps.sample_adaptive_offset_enabled_flag;
#ifdef AMT_SAO_MIN
                int curr_slice_id = 0; // issue in case of per slice SAO switch on/off supported
                if(doSao && !frame->m_slices[curr_slice_id].slice_sao_luma_flag && !frame->m_slices[curr_slice_id].slice_sao_chroma_flag)
                    doSao = false;
#endif
                if (!doSao) {
                    frame->m_ttWaitGpuPostProc.feiOp = frame->m_ttSubmitGpuPostProc.feiOp = MFX_FEI_H265_OP_DEBLOCK;
                }
            }
            // adjust PP
        }
    }


    frame->m_sliceQpY = (m_brc)
        ? GetRateQp(*frame, m_videoParam, m_brc)
        : GetConstQp(*frame, m_videoParam, inputParam->m_reencode);

    Ipp32s numCtb = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;
    memset(&frame->m_lcuQps[0], frame->m_sliceQpY, sizeof(frame->m_sliceQpY)*numCtb);
    if (m_videoParam.RegionIdP1 > 0)
        for (Ipp32s i = 0; i < numCtb << m_videoParam.Log2NumPartInCU; i++)
            frame->cu_data[i].predMode = MODE_INTRA;

    // setup slices
    H265Slice *currSlices = &frame->m_slices[0];
    for (Ipp8u i = 0; i < m_videoParam.NumSlices; i++) {
        (currSlices + i)->slice_qp_delta = frame->m_sliceQpY - m_pps.init_qp;
//        SetAllLambda(m_videoParam, (currSlices + i), frame->m_sliceQpY, frame);
        Ipp64f rd_lamba_multiplier;
        bool extraMult = SliceLambdaMultiplier(rd_lamba_multiplier, m_videoParam,  (currSlices + i)->slice_type, frame, 0, 0);
        SetSliceLambda(m_videoParam, currSlices + i, frame->m_sliceQpY, frame, rd_lamba_multiplier, extraMult);
    }
    
    if (m_videoParam.numRoi && m_videoParam.UseDQP)
        ApplyRoiDeltaQp(frame, m_videoParam);

    if (m_videoParam.DeltaQpMode && m_videoParam.UseDQP)
        ApplyDeltaQp(frame, m_videoParam, m_brc ? 1 : 0);

    frame->m_ttEncComplete.numDownstreamDependencies = 0;
    frame->m_ttEncComplete.numUpstreamDependencies = 1; // decremented when frame becomes a target frame
    frame->m_ttEncComplete.finished = 0;
    frame->m_ttEncComplete.poc = frame->m_frameOrder;


    m_frameEncoder[frame->m_encIdx]->SetEncodeFrame_GpuPostProc(frame, &m_pendingTasks);


    if (m_videoParam.enableCmFlag) {
        // the following modifications should be guarded by m_feiCritSect
        // because finished, numDownstreamDependencies and downstreamDependencies[]
        // may be accessed from FeiThreadRoutine()
        vm_mutex_lock(&m_feiCritSect);
        if (frame->m_ttSubmitGpuCopySrc.numUpstreamDependencies == 0) {
            m_feiSubmitTasks.push_back(&frame->m_ttSubmitGpuCopySrc); // GPU_COPY_SRC is independent
            vm_cond_signal(&m_feiCondVar);
        }
        for (Ipp32s i = 0; i < frame->m_numRefUnique; i++) {
            Ipp32s listIdx = frame->m_ttSubmitGpuMe[i].listIdx;
            Ipp32s refIdx = frame->m_ttSubmitGpuMe[i].refIdx;
            Frame *ref = frame->m_refPicList[listIdx].m_refFrames[refIdx];
            if (!ref->m_ttSubmitGpuCopyRec.finished)
                AddTaskDependency(&frame->m_ttSubmitGpuMe[i], &ref->m_ttSubmitGpuCopyRec); // GPU_SUBMIT_HME <- GPU_COPY_REF
        }
        m_outputQueue.splice(m_outputQueue.end(), m_encodeQueue, m_encodeQueue.begin());
        vm_mutex_unlock(&m_feiCritSect);
    } else {
        m_outputQueue.splice(m_outputQueue.end(), m_encodeQueue, m_encodeQueue.begin());
    }
}


mfxStatus H265Encoder::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, MFX_ENTRY_POINT &entryPoint)
{
    m_frameCountSync++;

    if (!surface) {
        if (m_frameCountBufferedSync == 0) // buffered frames to be encoded
            return MFX_ERR_MORE_DATA;
        m_frameCountBufferedSync--;
    }

    Ipp32s buffering = 0;
    if (!m_videoParam.encodedOrder) {
        buffering += m_videoParam.GopRefDist - 1;
        buffering += m_videoParam.m_framesInParallel - 1;
        if (m_videoParam.SceneCut || m_videoParam.AnalyzeCmplx || m_videoParam.DeltaQpMode) {
            buffering += 1;
            if (m_videoParam.SceneCut)     buffering += 10 + 1 + 1;
            if (m_videoParam.AnalyzeCmplx) buffering += m_videoParam.RateControlDepth;
            if (m_videoParam.DeltaQpMode) {
                if      (m_videoParam.DeltaQpMode&(AMT_DQP_PAQ|AMT_DQP_CAL)) buffering += 2 * m_videoParam.GopRefDist + 1;
                else if (m_videoParam.DeltaQpMode&(AMT_DQP_CAQ))             buffering += 1;
            }
        }
    }

#ifdef MFX_MAX_ENCODE_FRAMES
    if ((mfxI32)m_frameCountSync > MFX_MAX_ENCODE_FRAMES + buffering + 1)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
#endif // MFX_MAX_ENCODE_FRAMES

    mfxStatus status = MFX_ERR_NONE;
    if (surface && (Ipp32s)m_frameCountBufferedSync < buffering) {
        m_frameCountBufferedSync++;
        status = (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;
    }

    if (surface) {
        m_core.IncreaseReference(&surface->Data);
        MfxAutoMutex guard(m_statMutex);
        m_stat.NumCachedFrame++;
    }

    H265EncodeTaskInputParams *m_pTaskInputParams = (H265EncodeTaskInputParams*)H265_Malloc(sizeof(H265EncodeTaskInputParams));
    // MFX_ERR_MORE_DATA_RUN_TASK means that frame will be buffered and will be encoded later.
    // Output bitstream isn't required for this task. it is marker for TaskRoutine() and TaskComplete()
    m_pTaskInputParams->bs = (status == (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK) ? 0 : bs;
    m_pTaskInputParams->ctrl = ctrl;
    m_pTaskInputParams->surface = surface;
    m_pTaskInputParams->m_doStage = 0;
    m_pTaskInputParams->m_targetFrame = NULL;
    m_pTaskInputParams->m_threadCount = 0;
    m_pTaskInputParams->m_reencode = 0;

    entryPoint.pRoutine = TaskRoutine;
    entryPoint.pCompleteProc = TaskCompleteProc;
    entryPoint.pState = this;
    entryPoint.requiredNumThreads = m_videoParam.num_threads;
    entryPoint.pRoutineName = "EncodeHEVC";
    entryPoint.pParam = m_pTaskInputParams;

    return status;
}

// wait full complete of current task
#if defined( _WIN32) || defined(_WIN64)
#define thread_sleep(nms) Sleep(nms)
#else
#define thread_sleep(nms) _mm_pause()
#endif
#define x86_pause() _mm_pause()

mfxStatus H265Encoder::SyncOnFrameCompletion(H265EncodeTaskInputParams *inputParam)
{
    Frame *frame = inputParam->m_targetFrame;
    mfxBitstream *mfxBs = inputParam->bs;

    vm_mutex_lock(&m_critSect);
    vm_interlocked_cas32(&(inputParam->m_doStage), 5, 4);
    vm_mutex_unlock(&m_critSect);

    H265FrameEncoder *frameEnc = m_frameEncoder[frame->m_encIdx];
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

    overheadBytes = frameEnc->GetOutputData(bs);

    // BRC
    if (m_brc || m_videoParam.randomRepackThreshold) {
        const Ipp32s min_qp = 1;
        Ipp32s frameBytes = bs->DataLength - initialDataLength;
        mfxBRCStatus brcSts = m_brc ? m_brc->PostPackFrame(&m_videoParam, frame->m_sliceQpY, frame, frameBytes << 3, overheadBytes << 3, inputParam->m_reencode ? 1 : 0)
                                    : (myrand() < m_videoParam.randomRepackThreshold ? MFX_BRC_ERR_BIG_FRAME : MFX_BRC_OK);

        if (brcSts != MFX_BRC_OK ) {
            if ((brcSts & MFX_BRC_ERR_SMALL_FRAME) && (frame->m_sliceQpY < min_qp))
                brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;
            if (brcSts == MFX_BRC_ERR_BIG_FRAME && frame->m_sliceQpY == 51)
                brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;

            if (!(brcSts & MFX_BRC_NOT_ENOUGH_BUFFER)) {
                bs->DataLength = dataLength0;
                H265Bs_SetState(&frameEnc->m_bs[bs_main_id], pbs0, bitOffset0);
                overheadBytes = overheadBytes0;

                if (m_videoParam.enableCmFlag) {
                    vm_mutex_lock(&m_feiCritSect);
                    m_pauseFeiThread = 1;
                    vm_mutex_unlock(&m_feiCritSect);
                    while (m_feiThreadRunning) thread_sleep(0);
                    m_feiSubmitTasks.resize(0);
                    for (std::deque<ThreadingTask *>::iterator i = m_feiWaitTasks.begin(); i != m_feiWaitTasks.end(); ++i)
                        H265FEI_DestroySavedSyncPoint(m_fei, (*i)->syncpoint);
                    m_feiWaitTasks.resize(0);
                }

                while (m_threadingTaskRunning > 1) thread_sleep(0);
                m_pendingTasks.resize(0);
                m_encodeQueue.splice(m_encodeQueue.begin(), m_outputQueue);
                m_ttHubPool.ReleaseAll();

                PrepareToEncode(inputParam);

                if (m_videoParam.enableCmFlag) {
                    vm_mutex_lock(&m_feiCritSect);
                    m_pauseFeiThread = 0;
                    vm_mutex_unlock(&m_feiCritSect);
                    vm_cond_signal(&m_feiCondVar);
                }

                vm_interlocked_inc32(&inputParam->m_reencode);
                vm_interlocked_cas32(&(inputParam->m_doStage), 4, 5); // signal to restart!!!

                if (m_videoParam.num_threads > 1)
                    vm_cond_broadcast(&m_condVar);

                return MFX_TASK_WORKING; // recode!!!

            } else if (brcSts & MFX_BRC_ERR_SMALL_FRAME) {

                Ipp32s maxSize, minSize, bitsize = frameBytes << 3;
                Ipp8u *p = mfxBs->Data + mfxBs->DataOffset + mfxBs->DataLength;
                m_brc->GetMinMaxFrameSize(&minSize, &maxSize);

                // put rbsp_slice_segment_trailing_bits() which is a sequence of cabac_zero_words
                Ipp32s numTrailingBytes = IPP_MAX(0, ((minSize + 7) >> 3) - frameBytes);
                Ipp32s maxCabacZeroWords = (mfxBs->MaxLength - frameBytes) / 3;
                Ipp32s numCabacZeroWords = IPP_MIN(maxCabacZeroWords, (numTrailingBytes + 2) / 3);

                for (Ipp32s i = 0; i < numCabacZeroWords; i++) {
                    *p++ = 0x00;
                    *p++ = 0x00;
                    *p++ = 0x03;
                }
                bitsize += numCabacZeroWords * 24;

                m_brc->PostPackFrame(&m_videoParam,  frame->m_sliceQpY, frame, bitsize, (overheadBytes << 3) + bitsize - (frameBytes << 3), 1);
                mfxBs->DataLength += (bitsize >> 3) - frameBytes;

            } else {
                //return MFX_ERR_NOT_ENOUGH_BUFFER;
            }
        }
    }

    // bs update on completion stage
    vm_interlocked_cas32(&(inputParam->m_doStage), 4, 5);

    if (m_videoParam.hrdPresentFlag)
        m_hrd.Update((bs->DataLength - initialDataLength) * 8, *frame);

    if (m_videoParam.num_threads > 1)
        vm_cond_broadcast(&m_condVar);

    MfxAutoMutex guard(m_statMutex);
    m_stat.NumCachedFrame--;
    m_stat.NumFrame++;
    m_stat.NumBit += (bs->DataLength - initialDataLength) * 8;

    return MFX_TASK_DONE;
}


class OnExitHelperRoutine
{
public:
    OnExitHelperRoutine(volatile Ipp32u * arg) : m_arg(arg)
    {}
    ~OnExitHelperRoutine()
    {
        if (m_arg) {
            vm_interlocked_dec32(reinterpret_cast<volatile Ipp32u *>(m_arg));
        }
    }

private:
    volatile Ipp32u * m_arg;
};


namespace {
    Frame *FindFrameForLookaheadProcessing(std::list<Frame *> &queue)
    {
        for (FrameIter it = queue.begin(); it != queue.end(); it++)
            if (!(*it)->wasLAProcessed())
                return (*it);
        return NULL;
    }
};


void H265Encoder::PrepareToEncode(H265EncodeTaskInputParams *inputParam)
{
    // Set up qp and task dependencies.
    // Frames:
    //   newFrame    - frame to be allocated and copied from mfxFrameSurface1
    //   laFrame     - frame to be processed by look-ahead
    //   encFrame    - frame are being encoded (one of FramesInParallel frames)
    //   targetFrame - oldest frame of encFrame-s
    //   ref         - references frame used by one of encFrame-s, may be finished (then resides in m_dpb) or not (then resides in both m_outputQueue and m_dpb)
    //
    // Frame relations:
    //   laFrame may be newFrame
    //   laFrame may not be one of encFrame-s
    //   encFrame may be newFrame
    //   encFrame may be laFrame (only in case of low-delay encoding)
    //   targetFrame is one of encFrame-s
    //   ref may be one of encFrame-s
    //
    // Tasks dependencies:
    //   global.COMPLETE                  <- targetFrame.ENC_COMPLETE
    //   global.COMPLETE                  <- laFrame.TT_PREENC_END
    //   global.COMPLETE                  <- newFrame.INIT_NEW_FRAME
    //
    //   laFrame.TT_PREENC_START          <- laFrame.INIT_NEW_FRAME
    //
    //   encFrame.GPU_SUBMIT_COPY_SRC     <- encFrame.INIT_NEW_FRAME
    //   encFrame.GPU_SUBMIT_INTRA        <- encFrame.GPU_SUBMIT_COPY_SRC
    //   encFrame.GPU_WAIT_INTRA          <- encFrame.GPU_SUBMIT_INTRA
    //   encFrame.GPU_SUBMIT_ME[ref]      <- encFrame.GPU_SUBMIT_COPY_SRC
    //   encFrame.GPU_SUBMIT_ME[ref]      <- ref.GPU_SUBMIT_COPY_REF
    //   encFrame.GPU_WAIT_ME[ref]        <- encFrame.GPU_SUBMIT_ME[ref]
    //
    //   encFrame.ENCODE_CTU[row, 0]      <- ref.POST_PROC_CTU[row + lag, lastCol] (or ref.POST_PROC_ROW[row + lag] or ref.ENCODE_CTU[row + lag, lastCol])
    //   encFrame.ENCODE_CTU[0, 0]        <- encFrame.INIT_NEW_FRAME
    //   encFrame.ENCODE_CTU[0, 0]        <- encFrame.GPU_WAIT_INTRA
    //   encFrame.ENCODE_CTU[0, 0]        <- encFrame.GPU_WAIT_ME[ref]
    //   encFrame.ENCODE_CTU[row, col]    <- encFrame.ENCODE_CTU[row, col - 1]
    //   encFrame.ENCODE_CTU[row, col]    <- encFrame.ENCODE_CTU[row - 1, col + 1]
    //
    //OLD (CPU_POST_PROC)
    //   encFrame.POST_PROC_CTU[row, col] <- encFrame.POST_PROC_CTU[row, col - 1]
    //   encFrame.POST_PROC_CTU[row, col] <- encFrame.POST_PROC_CTU[row - 1, col + 1]
    //   encFrame.POST_PROC_CTU[row, col] <- encFrame.ENCODE_CTU[row, col]
    //   encFrame.POST_PROC_CTU[row, col] <- encFrame.ENCODE_CTU[row, col + 1]
    //   encFrame.POST_PROC_CTU[row, col] <- encFrame.ENCODE_CTU[row + 1, col]
    //   encFrame.POST_PROC_CTU[row, col] <- encFrame.ENCODE_CTU[row + 1, col + 1]
    //   encFrame.POST_PROC_ROW[row]      <- encFrame.POST_PROC_ROW[row - 1]
    //NEW (GPU_PP)
    //   encFrame.GPU_SUBMIT_POST_PROC    <- encFrame.ENCODE_CTU[lastRow, lastCol]
    //
    //OLD (CPU_POST_PROC)
    //   encFrame.ENC_COMPLETE            <- encFrame.POST_PROC_CTU[lastRow, lastCol] (or encFrame.POST_PROC_ROW[lastRow] or encFrame.ENCODE_CTU[lastRow, lastCol])
    //   encFrame.GPU_SUBMIT_COPY_REF     <- encFrame.POST_PROC_CTU[lastRow, lastCol] (or encFrame.POST_PROC_ROW[lastRow] or encFrame.ENCODE_CTU[lastRow, lastCol])
    //NEW (GPU_PP)
    //   encFrame.ENC_COMPLETE            <- encFrame.CPU_COPY_REF
    //   encFrame.CPU_COPY_REF            <- encFrame.GPU_WAIT_POST_PROC (or encFrame.ENCODE_CTU[lastRow, lastCol])
    //
    // May be finished and should not be rerun in case of repack:
    //    newFrame.INIT_NEW_FRAME
    //    laFrame.TT_PREENC_END (if finished don't reenqueue TT_PREENC_START, TT_PREENC_ROUTINE and TT_PREENC_END, otherwise reenqueue all of them

    m_ttComplete.InitComplete(-1);

    if (m_newFrame && !m_newFrame->m_ttInitNewFrame.finished) {
        m_newFrame->m_ttInitNewFrame.InitNewFrame(m_newFrame, inputParam->surface, m_newFrame->m_frameOrder);
        AddTaskDependency(&m_ttComplete, &m_newFrame->m_ttInitNewFrame); // COMPLETE <- INIT_NEW_FRAME
    }

    if (m_laFrame && !m_la->m_threadingTasks.back().finished) {
        if (m_la->SetFrame(m_laFrame) == 1) {
            AddTaskDependency(&m_ttComplete, &m_la->m_threadingTasks.back()); // COMPLETE <- TT_PREENC_END
            if (m_laFrame && !m_laFrame->m_ttInitNewFrame.finished)
                AddTaskDependency(&m_la->m_threadingTasks.front(), &m_la->m_frame->m_ttInitNewFrame); // TT_PREENC_START <- INIT_NEW_FRAME
        }
    }

    if (inputParam->bs) {
        while (m_outputQueue.size() < (size_t)m_videoParam.m_framesInParallel && !m_encodeQueue.empty())
            EnqueueFrameEncoder(inputParam);

        inputParam->m_targetFrame = *m_outputQueue.begin();
        m_ttComplete.poc = inputParam->m_targetFrame->m_frameOrder;
        AddTaskDependency(&m_ttComplete, &inputParam->m_targetFrame->m_ttEncComplete); // COMPLETE <- ENC_COMPLETE
    }


    {
        vm_mutex_lock(&m_critSect);

        if (m_videoParam.enableCmFlag && inputParam->m_targetFrame) {
            Frame *frame = inputParam->m_targetFrame;
            if ((frame->m_isRef || m_videoParam.enableCmPostProc && m_videoParam.doDumpRecon && frame->m_doPostProc)) {
                if (frame->m_isRef && m_videoParam.enableCmPostProc && frame->m_doPostProc) {
                    if (!frame->m_ttPadRecon.finished)
                        AddTaskDependency(&m_ttComplete, &frame->m_ttPadRecon); // COMPLETE <- PAD_RECON
                }
                else {
                    if (!frame->m_ttWaitGpuCopyRec.finished)
                        AddTaskDependency(&m_ttComplete, &frame->m_ttWaitGpuCopyRec); // COMPLETE <- GPU_WAIT_COPY_REC
                }
            }
        }

        if (m_newFrame && !m_newFrame->m_ttInitNewFrame.finished && m_newFrame->m_ttInitNewFrame.numUpstreamDependencies == 0)
            m_pendingTasks.push_back(&m_newFrame->m_ttInitNewFrame); // INIT_NEW_FRAME is independent

        if (m_laFrame && !m_la->m_threadingTasks.front().finished && m_la->m_threadingTasks.front().numUpstreamDependencies == 0)
            m_pendingTasks.push_back(&m_la->m_threadingTasks.front()); // LA_START is independent

        if (m_ttComplete.numUpstreamDependencies == 0)
            m_pendingTasks.push_front(&m_ttComplete); // COMPLETE's dependencies are already resolved

        if (inputParam->bs && vm_interlocked_dec32(&inputParam->m_targetFrame->m_ttEncComplete.numUpstreamDependencies) == 0)
            m_pendingTasks.push_front(&inputParam->m_targetFrame->m_ttEncComplete); // targetFrame.ENC_COMPLETE's dependencies are already resolved

        vm_mutex_unlock(&m_critSect);
    }
}


mfxStatus H265Encoder::TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    H265ENC_UNREFERENCED_PARAMETER(threadNumber);
    H265ENC_UNREFERENCED_PARAMETER(callNumber);

    if (pState == NULL || pParam == NULL) {
        return MFX_ERR_NULL_PTR;
    }

    H265Encoder *th = static_cast<H265Encoder *>(pState);
    H265EncodeTaskInputParams *inputParam = (H265EncodeTaskInputParams*)pParam;
    mfxStatus sts = MFX_ERR_NONE;

    Ipp32s stage = vm_interlocked_cas32(&inputParam->m_doStage, 1, 0);
    if (0 == stage) {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "PrepareToEncode");
        th->m_newFrame = th->AcceptFrame(inputParam->surface, inputParam->ctrl, inputParam->bs);
        if (th->m_la.get()) {
            th->m_laFrame = FindFrameForLookaheadProcessing(th->m_inputQueue);
            th->m_la->m_threadingTasks.back().finished = 0;
        }

        th->PrepareToEncode(inputParam);

        vm_mutex_lock(&th->m_critSect);
        inputParam->m_doStage = 4;
        Ipp32s numPendingTasks = th->m_pendingTasks.size();
        vm_mutex_unlock(&th->m_critSect);
        while (numPendingTasks--)
            vm_cond_signal(&th->m_condVar);
    }

    // global thread count control
    Ipp32u newThreadCount = vm_interlocked_inc32(&inputParam->m_threadCount);
    OnExitHelperRoutine onExitHelper(&inputParam->m_threadCount);
    if (newThreadCount > th->m_videoParam.num_threads)
        return MFX_TASK_BUSY;

#if TT_TRACE
    char mainname[256] = "TaskRoutine ";
    if (inputParam->m_targetFrame)
        strcat(mainname, NUMBERS[inputParam->m_targetFrame->m_frameOrder & 0x7f]);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, mainname);
#endif // TT_TRACE

    ThreadingTask *taskNext = NULL;
    Ipp32u reencodeCounter = 0;
    while (true) {
        vm_mutex_lock(&th->m_critSect);
        while (1) {
            if (taskNext && reencodeCounter != inputParam->m_reencode)
                taskNext = NULL;
            if (inputParam->m_doStage == 4 && (taskNext || th->m_pendingTasks.size()))
                break;
            if (inputParam->m_doStage >= 6)
                break;
            // temp workaround, should be cond_wait. Possibly linux kernel futex bug (not sure yet)
            vm_cond_wait(&th->m_condVar, &th->m_critSect/*, 1*/);
        }

        if (inputParam->m_doStage >= 6) {
            if (taskNext)
                th->m_pendingTasks.push_back(taskNext);
            vm_mutex_unlock(&th->m_critSect);
            break;
        }

        ThreadingTask *task = NULL;
        if (taskNext) {
            task = taskNext;
        } else {
            std::deque<ThreadingTask *>::iterator t = th->m_pendingTasks.begin();
            if (th->m_videoParam.enableCmFlag) {
                if ((*t)->action == TT_ENCODE_CTU || (*t)->action == TT_POST_PROC_CTU || (*t)->action == TT_POST_PROC_ROW || (*t)->action == TT_PAD_RECON) {
                    std::deque<ThreadingTask *>::iterator i = t;
                    for (std::deque<ThreadingTask *>::iterator i = ++th->m_pendingTasks.begin(); i != th->m_pendingTasks.end(); ++i) {
                        if ((*i)->action == TT_ENCODE_CTU || (*i)->action == TT_POST_PROC_CTU || (*i)->action == TT_POST_PROC_ROW || (*i)->action == TT_PAD_RECON) {
                            Frame *iframe = ((*i)->action == TT_PAD_RECON ? (*i)->frame : (*i)->fenc->m_frame);
                            Frame *tframe = ((*t)->action == TT_PAD_RECON ? (*t)->frame : (*t)->fenc->m_frame);
                            if (tframe != inputParam->m_targetFrame && iframe == inputParam->m_targetFrame ||
                                iframe->m_pyramidLayer  < tframe->m_pyramidLayer ||
                                iframe->m_pyramidLayer == tframe->m_pyramidLayer && iframe->m_encOrder < tframe->m_encOrder)
                                t = i;
                            //if ((*i)->fenc->m_frame->m_isRef && !(*t)->fenc->m_frame->m_isRef)
                            //    t = i;
                            //else if ((*i)->fenc->m_frame->m_isRef == (*t)->fenc->m_frame->m_isRef && (*i)->fenc->m_frame->m_encOrder < (*t)->fenc->m_frame->m_encOrder)
                            //    t = i;
                        }
                    }
                }
            }
            task = *t;
            th->m_pendingTasks.erase(t);
        }

        vm_interlocked_inc32(&th->m_threadingTaskRunning);

        taskNext = NULL;
        vm_mutex_unlock(&th->m_critSect);

        Ipp32s taskCount = 0;
        ThreadingTask *taskDepAll[MAX_NUM_DEPENDENCIES];

        Ipp32s distBest;

#if TT_TRACE
        char ttname[256] = "";
        strcat(ttname, TT_NAMES[task->action]);
        strcat(ttname, " ");
        strcat(ttname, NUMBERS[task->poc & 0x7f]);
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, ttname);
#endif // TT_TRACE

        sts = MFX_TASK_DONE;
        try {
            switch (task->action) {
            case TT_INIT_NEW_FRAME:
                th->InitNewFrame(task->frame, task->indata);
                break;
            case TT_PAD_RECON:
                th->PerformPadRecon(task->frame);
                break;
            case TT_PREENC_START:
            case TT_PREENC_ROUTINE:
            case TT_PREENC_END:
                task->la->PerformThreadingTask(task->action, task->row, task->col);
                break;
            case TT_HUB:
                break;
            case TT_ENCODE_CTU:
            case TT_POST_PROC_CTU:
            case TT_POST_PROC_ROW:
                sts = th->m_videoParam.bitDepthLuma > 8 ?
                    task->fenc->PerformThreadingTask<Ipp16u>(task->action, task->row, task->col):
                    task->fenc->PerformThreadingTask<Ipp8u>(task->action, task->row, task->col);
                if (sts != MFX_TASK_DONE) {
                    // shouldn't happen
                    assert(0);
                }
                break;
            case TT_ENC_COMPLETE:
                sts = th->SyncOnFrameCompletion(inputParam);
                if (sts != MFX_TASK_DONE) {
                    vm_interlocked_dec32(&th->m_threadingTaskRunning);
                    continue;
                }
                break;
            case TT_COMPLETE:
                vm_mutex_lock(&th->m_critSect);
                vm_interlocked_cas32(&(inputParam->m_doStage), 6, 4);
                vm_mutex_unlock(&th->m_critSect);
                vm_cond_broadcast(&th->m_condVar);
                break;
            default:
                assert(0);
                break;
            }
        } catch (...) {
            // to prevent SyncOnFrameCompletion hang
            vm_interlocked_dec32(&th->m_threadingTaskRunning);
            vm_mutex_lock(&th->m_critSect);
            inputParam->m_doStage = 6;
            vm_mutex_unlock(&th->m_critSect);
            vm_cond_broadcast(&th->m_condVar);
            throw;
        }

#if TT_TRACE
        MFX_AUTO_TRACE_STOP();
#endif // TT_TRACE

        task->finished = 1;

        distBest = -1;

        for (int i = 0; i < task->numDownstreamDependencies; i++) {
            ThreadingTask *taskDep = task->downstreamDependencies[i];
            if (vm_interlocked_dec32(&taskDep->numUpstreamDependencies) == 0) {
                if (taskDep->action == TT_GPU_SUBMIT) {
                    vm_mutex_lock(&th->m_feiCritSect);
                    th->m_feiSubmitTasks.push_back(taskDep);
                    vm_mutex_unlock(&th->m_feiCritSect);
                    vm_cond_signal(&th->m_feiCondVar);
                } else if (taskDep->action != TT_COMPLETE &&
                           taskDep->action != TT_ENC_COMPLETE &&
                           taskDep->poc != task->poc) {
                    taskDepAll[taskCount++] = taskDep;
                } else {
                    Ipp32s dist = (abs(task->row - taskDep->row) << 8) + abs(task->col - taskDep->col) + 3;
                    if (dist == ((1 << 8) + 1) && task->action == TT_ENCODE_CTU && taskDep->action == TT_POST_PROC_CTU)
                        dist = 2;
                    else if (taskDep->action == TT_ENC_COMPLETE)
                        dist = 0;
                    else if (taskDep->action == TT_COMPLETE)
                        dist = 1;

                    if (distBest < 0 || dist < distBest) {
                        distBest = dist;
                        if (taskNext)
                            taskDepAll[taskCount++] = taskNext;
                        taskNext = taskDep;
                    } else
                        taskDepAll[taskCount++] = taskDep;
                }
            }
        }

        if (taskCount) {
            int c;
            vm_mutex_lock(&th->m_critSect);
            for (c = 0; c < taskCount; c++) {
                th->m_pendingTasks.push_back(taskDepAll[c]);
            }
            vm_mutex_unlock(&th->m_critSect);
            for (c = 0; c < taskCount; c++)
                vm_cond_signal(&th->m_condVar);
        }

        reencodeCounter = inputParam->m_reencode;
        vm_interlocked_dec32(&th->m_threadingTaskRunning);
    }

    return MFX_TASK_DONE;
}


mfxStatus H265Encoder::TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes)
{
    H265ENC_UNREFERENCED_PARAMETER(taskRes);
    MFX_CHECK_NULL_PTR1(pState);
    MFX_CHECK_NULL_PTR1(pParam);

    H265Encoder *th = static_cast<H265Encoder *>(pState);
    H265EncodeTaskInputParams *inputParam = static_cast<H265EncodeTaskInputParams*>(pParam);
    mfxBitstream *bs = inputParam->bs;

    if (bs) {
        Frame* coded = inputParam->m_targetFrame;
        H265FrameEncoder* frameEnc = th->m_frameEncoder[coded->m_encIdx];

        th->OnEncodingQueried(coded); // remove coded frame from encodeQueue (outputQueue) and clean (release) dpb.
        frameEnc->SetFree(true);
    }
    if (th->m_laFrame)
        th->m_laFrame->setWasLAProcessed();
    th->OnLookaheadCompletion();

    H265_Free(pParam);

    return MFX_TASK_DONE;
}

void H265Encoder::InitNewFrame(Frame *out, mfxFrameSurface1 *inExternal)
{
    mfxFrameAllocator &fa = m_core.FrameAllocator();
    mfxFrameSurface1 in = *inExternal;
    mfxStatus st = MFX_ERR_NONE;

    // attach original surface to frame
    out->m_origin = m_inputFrameDataPool.Allocate();

    if (m_videoParam.inputVideoMem) { // copy from d3d to internal frame in system memory
        if (m_videoParam.enableCmFlag) {
            mfxHDLPair videoMemoryHandlePlaceHolder = {};
            mfxHDL &videoMemHandle = videoMemoryHandlePlaceHolder.first;
            if ((st = fa.GetHDL(fa.pthis, in.Data.MemId, &videoMemHandle)) != MFX_ERR_NONE)
                Throw(std::runtime_error("GetHDL failed"));
            if ((st = m_cmcopy->Copy(videoMemHandle, out->m_origin->y, out->m_origin->uv)) != MFX_ERR_NONE)
                Throw(std::runtime_error("CmCopy::Copy failed"));
        } else {
            if ((st = fa.Lock(fa.pthis,in.Data.MemId, &in.Data)) != MFX_ERR_NONE)
                Throw(std::runtime_error("LockExternalFrame failed"));
            Ipp32s bpp = (m_videoParam.fourcc == P010 || m_videoParam.fourcc == P210) ? 2 : 1;
            Ipp32s width = out->m_origin->width * bpp;
            Ipp32s height = out->m_origin->height;
            IppiSize roi = { width, height };
            ippiCopyManaged_8u_C1R(in.Data.Y, in.Data.Pitch, out->m_origin->y, out->m_origin->pitch_luma_bytes, roi, 2);
            roi.height >>= m_videoParam.chromaShiftH;
            ippiCopyManaged_8u_C1R(in.Data.Y + height * in.Data.Pitch, in.Data.Pitch, out->m_origin->uv, out->m_origin->pitch_chroma_bytes, roi, 2);
            fa.Unlock(fa.pthis, in.Data.MemId, &in.Data);
        }
    } else {
        bool locked = false;
        if (in.Data.Y == 0) {
            if ((st = fa.Lock(fa.pthis,in.Data.MemId, &in.Data)) != MFX_ERR_NONE)
                Throw(std::runtime_error("LockExternalFrame failed"));
            locked = true;
        }
        out->CopyFrameData(&in);
        if (locked)
            fa.Unlock(fa.pthis, in.Data.MemId, &in.Data);
    }
    m_core.DecreaseReference(&inExternal->Data); // do it here

    if (m_videoParam.doDumpSource && (m_videoParam.fourcc == MFX_FOURCC_NV12 || m_videoParam.fourcc == MFX_FOURCC_P010)) {
        if (vm_file *f = vm_file_fopen(m_videoParam.sourceDumpFileName, (out->m_frameOrder == 0) ? VM_STRING("wb") : VM_STRING("ab"))) {
            Ipp32s luSz = (m_videoParam.bitDepthLuma == 8) ? 1 : 2;
            Ipp32s luW = m_videoParam.Width - m_videoParam.CropLeft - m_videoParam.CropRight;
            Ipp32s luH = m_videoParam.Height - m_videoParam.CropTop - m_videoParam.CropBottom;
            Ipp32s luPitch = out->m_origin->pitch_luma_bytes;
            Ipp8u *luPtr = out->m_origin->y + (m_videoParam.CropTop * luPitch + m_videoParam.CropLeft * luSz);
            for (Ipp32s y = 0; y < luH; y++, luPtr += luPitch)
                vm_file_fwrite(luPtr, luSz, luW, f);

            Ipp32s chSz = (m_videoParam.bitDepthChroma == 8) ? 1 : 2;
            Ipp32s chW = luW;
            Ipp32s chH = luH >> 1;
            Ipp32s chPitch = out->m_origin->pitch_luma_bytes;
            Ipp8u *chPtr = out->m_origin->uv + (m_videoParam.CropTop / 2 * chPitch + m_videoParam.CropLeft * chSz);
            for (Ipp32s y = 0; y < chH; y++, chPtr += chPitch)
                vm_file_fwrite(chPtr, chSz, chW, f);

            vm_file_fclose(f);
        }
    }

    // for luma need 1 pix for gradient analysis and full padding for GACC
    // for chroma need no padding
    PadRectLuma(*out->m_origin, m_videoParam.fourcc, 0, 0, out->m_origin->width, out->m_origin->height);

        // attach lowres surface to frame
    if (m_la.get() && (m_videoParam.LowresFactor || m_videoParam.SceneCut))
        out->m_lowres = m_frameDataLowresPool.Allocate();

    // attach statistics to frame
    if (m_la.get()) {
        if (m_videoParam.LowresFactor) {
            out->m_stats[1] = m_statsLowresPool.Allocate();
            out->m_stats[1]->ResetAvgMetrics();
        }
        if (m_videoParam.DeltaQpMode || m_videoParam.LowresFactor == 0) {
            out->m_stats[0] = m_statsPool.Allocate();
            out->m_stats[0]->ResetAvgMetrics();
        }
    }

    // each new frame should be analysed by lookahead algorithms family.
    Ipp32u ownerCount = (m_videoParam.DeltaQpMode ? 1 : 0) + (m_videoParam.SceneCut ? 1 : 0) + (m_videoParam.AnalyzeCmplx ? 1 : 0);
    out->m_lookaheadRefCounter = ownerCount;
}

void H265Encoder::PerformPadRecon(Frame *frame)
{
    PadRectLumaAndChroma(*frame->m_recon, m_videoParam.fourcc, 0, 0, frame->m_recon->width, frame->m_recon->height);
}

Ipp32u VM_THREAD_CALLCONVENTION H265Encoder::FeiThreadRoutineStarter(void *p)
{
    ((H265Encoder *)p)->FeiThreadRoutine();
    return 0;
}


void H265Encoder::FeiThreadRoutine()
{
    try {
        while (true) {
            ThreadingTask *task = NULL;

            vm_mutex_lock(&m_feiCritSect);

            while (m_stopFeiThread == 0 && (m_pauseFeiThread == 1 || m_feiSubmitTasks.empty() && m_feiWaitTasks.empty()))
                vm_cond_wait(&m_feiCondVar, &m_feiCritSect);

            if (m_stopFeiThread) {
                vm_mutex_unlock(&m_feiCritSect);
                break;
            }

            if (!m_feiSubmitTasks.empty()) {
                Frame *targetFrame = m_outputQueue.front();
                std::deque<ThreadingTask *>::iterator t = m_feiSubmitTasks.begin();
                std::deque<ThreadingTask *>::iterator i = ++m_feiSubmitTasks.begin();
                std::deque<ThreadingTask *>::iterator e = m_feiSubmitTasks.end();
                for (; i != e; ++i) {
                    if ((*i)->frame == targetFrame && (*t)->frame != targetFrame ||
                        (*i)->frame->m_pyramidLayer < (*t)->frame->m_pyramidLayer ||
                        (*i)->frame->m_pyramidLayer == (*t)->frame->m_pyramidLayer && (*i)->frame->m_encOrder < (*t)->frame->m_encOrder)
                        t = i;
                }
                task = (*t);
                m_feiSubmitTasks.erase(t);
            }

            m_feiThreadRunning = 1;
            vm_mutex_unlock(&m_feiCritSect);

            if (task) {
                while (m_feiWaitTasks.size() > 0)
                    if (!FeiThreadWait(0))
                        break;
                FeiThreadSubmit(*task);
            } else {
                assert(!m_feiWaitTasks.empty());
                if (!FeiThreadWait(2000))
                    Throw(std::runtime_error(""));
            }
            m_feiThreadRunning = 0;
        }
    } catch (...) {
        assert(!"FeiThreadRoutine failed with exception");
        m_feiThreadRunning = 0;
        return;
    }
}

struct DeblockParam // size = 336 B = 84 DW
{
    Ipp8u  tabBeta[52];            // +0 B
    Ipp16u Width;                  // +26 W
    Ipp16u Height;                 // +27 W
    Ipp16u PicWidthInCtbs;         // +28 W
    Ipp16u PicHeightInCtbs;        // +29 W
    Ipp16s tcOffset;               // +30 W
    Ipp16s betaOffset;             // +31 W
    Ipp8u  chromaQp[58];           // +64 B
    Ipp8u  crossSliceBoundaryFlag; // +122 B
    Ipp8u  crossTileBoundaryFlag;  // +123 B
    Ipp8u  TULog2MinSize;          // +124 B
    Ipp8u  MaxCUDepth;             // +125 B
    Ipp8u  Log2MaxCUSize;          // +126 B
    Ipp8u  Log2NumPartInCU;        // +127 B
    Ipp8u  tabTc[54];              // +128 B
    Ipp8u  MaxCUSize;              // +182 B
    Ipp8u  chromaFormatIdc;        // +183 B
    Ipp8u  alignment0[8];          // +184 B
    Ipp32s list0[16];              // +48 DW
    Ipp32s list1[16];              // +64 DW
    Ipp8u  scan2z[16];             // +320 B
};

const Ipp8u h265_QPtoChromaQP[3][58]=
{
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 29, 30, 31, 32,
        33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 39, 40, 41, 42, 43, 44,
        45, 46, 47, 48, 49, 50, 51
    },
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
        51, 51, 51, 51, 51, 51, 51
    },
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
        51, 51, 51, 51, 51, 51, 51
     }
};

void FillDeblockParam(DeblockParam & deblockParam, const H265Enc::H265VideoParam & videoParam, const int list0[33], const int list1[33])
{
    static Ipp32s tcTable[] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
        3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9, 10, 11, 13,
        14, 16, 18, 20, 22, 24
    };

    static Ipp32s betaTable[] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
        26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
        58, 60, 62, 64
    };

    deblockParam.Width  = videoParam.Width;
    deblockParam.Height = videoParam.Height;
    deblockParam.PicWidthInCtbs = videoParam.PicWidthInCtbs;
    deblockParam.PicHeightInCtbs = videoParam.PicHeightInCtbs;
    deblockParam.tcOffset = 0;//videoParam.tcOffset;
    deblockParam.betaOffset = 0;//videoParam.betaOffset;
    deblockParam.crossSliceBoundaryFlag = 0;//videoParam.crossSliceBoundaryFlag;
    deblockParam.crossTileBoundaryFlag = 0;//videoParam.crossTileBoundaryFlag;
    deblockParam.TULog2MinSize = videoParam.QuadtreeTULog2MinSize;
    deblockParam.MaxCUDepth = videoParam.MaxCUDepth;
    deblockParam.Log2MaxCUSize = videoParam.Log2MaxCUSize;
    deblockParam.Log2NumPartInCU = videoParam.Log2NumPartInCU;
    deblockParam.MaxCUSize = videoParam.MaxCUSize;
    deblockParam.chromaFormatIdc = videoParam.chromaFormatIdc;

    memcpy(deblockParam.list0, list0, sizeof(deblockParam.list0));
    memcpy(deblockParam.list1, list1, sizeof(deblockParam.list1));
    memcpy(deblockParam.scan2z, h265_scan_r2z4, sizeof(deblockParam.scan2z));

    for (int i = 0; i < 54; i++)
        deblockParam.tabTc[i] = tcTable[i];
    for (int i = 0; i < 52; i++)
        deblockParam.tabBeta[i] = betaTable[i];
    for (int i = 0; i < 58; i++)
        deblockParam.chromaQp[i] = h265_QPtoChromaQP[0][i];
}

struct SaoVideoParam
{
    Ipp32s Width;
    Ipp32s Height;
    Ipp32s PicWidthInCtbs;
    Ipp32s PicHeightInCtbs;

    Ipp32s chromaFormatIdc;
    Ipp32s MaxCUSize;
    Ipp32s bitDepthLuma;
    Ipp32s saoOpt;

    Ipp32f m_rdLambda;
    Ipp32s SAOChromaFlag;
    Ipp32s enableBandOffset;
    Ipp32s reserved2;
}; // // sizeof == 48 bytes


void SetSaoVideoParam(SaoVideoParam & sao, const H265VideoParam & par, Ipp32f lambda)
{
    sao.Width = par.Width;
    sao.Height= par.Height;

    sao.MaxCUSize = par.MaxCUSize;

    sao.PicWidthInCtbs  = par.PicWidthInCtbs;
    sao.PicHeightInCtbs = par.PicHeightInCtbs;

    sao.chromaFormatIdc = par.chromaFormatIdc;

    sao.bitDepthLuma = par.bitDepthLuma;

    sao.saoOpt = par.saoOpt;
    sao.SAOChromaFlag = par.SAOChromaFlag;

    sao.m_rdLambda = lambda;
    sao.enableBandOffset = (par.saoOpt == SAO_OPT_ALL_MODES) ? 1 : 0;
}

void SetPostProcParam(PostProcParam & postprocParam, const H265Enc::H265VideoParam & videoParam, const int list0[33], const int list1[33], Ipp32f lambda)
{
    static Ipp32s tcTable[] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
        3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9, 10, 11, 13,
        14, 16, 18, 20, 22, 24
    };

    static Ipp32s betaTable[] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
        26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
        58, 60, 62, 64
    };

    postprocParam.Width  = videoParam.Width;
    postprocParam.Height = videoParam.Height;
    postprocParam.PicWidthInCtbs = videoParam.PicWidthInCtbs;
    postprocParam.PicHeightInCtbs = videoParam.PicHeightInCtbs;
    postprocParam.tcOffset = 0;//videoParam.tcOffset;
    postprocParam.betaOffset = 0;//videoParam.betaOffset;
    postprocParam.crossSliceBoundaryFlag = 0;//videoParam.crossSliceBoundaryFlag;
    postprocParam.crossTileBoundaryFlag = 0;//videoParam.crossTileBoundaryFlag;
    postprocParam.TULog2MinSize = videoParam.QuadtreeTULog2MinSize;
    postprocParam.MaxCUDepth = videoParam.MaxCUDepth;
    postprocParam.Log2MaxCUSize = videoParam.Log2MaxCUSize;
    postprocParam.Log2NumPartInCU = videoParam.Log2NumPartInCU;
    postprocParam.MaxCUSize = videoParam.MaxCUSize;
    postprocParam.chromaFormatIdc = videoParam.chromaFormatIdc;

    memcpy(postprocParam.list0, list0, sizeof(postprocParam.list0));
    memcpy(postprocParam.list1, list1, sizeof(postprocParam.list1));
    memcpy(postprocParam.scan2z, h265_scan_r2z4, sizeof(postprocParam.scan2z));

    for (int i = 0; i < 54; i++)
        postprocParam.tabTc[i] = tcTable[i];
    for (int i = 0; i < 52; i++)
        postprocParam.tabBeta[i] = betaTable[i];
    for (int i = 0; i < 58; i++)
        postprocParam.chromaQp[i] = h265_QPtoChromaQP[0][i];

    // sao extension (carefully!!! saoChroma is configured on Fei.Init() )
    postprocParam.SAOChromaFlag = videoParam.SAOChromaFlag;

    postprocParam.m_rdLambda = lambda;
    postprocParam.enableBandOffset = (videoParam.saoOpt == SAO_OPT_ALL_MODES) ? 1 : 0;

    // set slice/tile segmentation to prevent invalid sao merge mode
    const Ipp32s maxWidth  = 3840;
    const Ipp32s maxHeight = 2160;
    const Ipp32s numCtbX   = maxWidth / MAX_CU_SIZE;
    const Ipp32s numCtbY   = maxHeight / MAX_CU_SIZE;

    memset(postprocParam.availLeftAbove, 1, numCtbX + numCtbY);

    // tile
    for (Ipp32s i = 0; i < CodecLimits::MAX_NUM_TILE_COLS; i++) {
        postprocParam.availLeftAbove[ videoParam.tileColStart[i] ] = 0;
    }
    for (Ipp32s i = 0; i < CodecLimits::MAX_NUM_TILE_ROWS; i++) {
        postprocParam.availLeftAbove[ numCtbX + videoParam.tileRowStart[i] ] = 0;
    }

    // slice
    for (Ipp32s i = 0; i < videoParam.NumSlices; i++) {
        Ipp32u ctbY = videoParam.m_slices[i].first_ctb_addr / videoParam.PicWidthInCtbs;
        postprocParam.availLeftAbove[ numCtbX + ctbY ] = 0;
    }

    // LumaOffset for block based SaoStatistics
    {
        Ipp32s blockW = 16;
        Ipp32s blockH = 16;
        mfxU32 tsWidth   = (videoParam.Width  + videoParam.MaxCUSize - 1) / videoParam.MaxCUSize * (videoParam.MaxCUSize / blockW);
        mfxU32 tsHeight  = (videoParam.Height + videoParam.MaxCUSize - 1) / videoParam.MaxCUSize * (videoParam.MaxCUSize / blockH);
        mfxU32 numThreads = tsWidth * tsHeight;
        mfxU32 bufsize = numThreads * (16+16+32+32) * sizeof(Ipp16s);
        postprocParam.offsetChroma = bufsize;
    }
}

Ipp32u GetDist(FeiOutData **data, Ipp32s size, Ipp32s x, Ipp32s y, Ipp32s distIdx)
{
    assert(size < 4);
    assert((size == 3 || size == 2) && distIdx < 9 || (size == 1 || size == 0) && distIdx == 0);
    Ipp32u pitch = data[size]->m_pitch;
    Ipp8u *p = data[size]->m_sysmem;
    Ipp32s recordSize = (size == 3 || size == 2) ? 64 : 8;
    return *(Ipp32u *)(p + y * pitch + x * recordSize + 4 + distIdx * sizeof(Ipp32u));
}

mfxI16Pair GetMv(FeiOutData **data, Ipp32s size, Ipp32s x, Ipp32s y)
{
    assert(size < 4);
    Ipp32u pitch = data[size]->m_pitch;
    Ipp8u *p = data[size]->m_sysmem;
    Ipp32s recordSize = (size == 3 || size == 2) ? 64 : 8;
    return *(mfxI16Pair *)(p + y * pitch + x * recordSize);
}

void DumpGpuMeData(const ThreadingTask &task, H265VideoParam &par) {
    Ipp32s poc = task.frame->m_frameOrder;
    Ipp32s refpoc = task.frame->m_refPicList[task.listIdx].m_refFrames[task.refIdx]->m_frameOrder;
    Ipp32s uniqRefIdx = task.frame->m_mapListRefUnique[task.listIdx][task.refIdx];

    FeiOutData **data = task.frame->m_feiInterData[uniqRefIdx];

    Ipp32s width = par.Width;
    Ipp32s height = par.Height;
    Ipp32s widthInCtb = (par.Width + 63) / 64;
    Ipp32s heightInCtb = (par.Height + 63) / 64;

    char fnameMv[256] = {};
    char fnameDist[256] = {};
    sprintf(fnameMv, "frame%02d_ref%02d_mv.txt", poc, refpoc);
    //sprintf(fnameDist, "frame%02d_ref%02d_dist.txt", poc, refpoc);

    FILE *fileMv = fopen(fnameMv, "w");
    FILE *fileDist = fopen(fnameDist, "w");

    for (Ipp32s ctby = 0; ctby < heightInCtb; ctby++) {
        for (Ipp32s ctbx = 0; ctbx < widthInCtb; ctbx++) {
            fprintf(fileMv, "CTB addr=%d, col=%d, row=%d\n", ctbx + widthInCtb * ctby, ctbx, ctby);
            for (Ipp32s size = 3; size >= 0; size--) {
                Ipp32s blockSize = 1 << (3 + size);
                Ipp32s ctbSizeInBlocks = 1 << (3 - size);
                fprintf(fileMv, "  %dx%d\n", blockSize, blockSize);
                for (Ipp32s y = 0; y < ctbSizeInBlocks; y++) {
                    fprintf(fileMv, "     ");
                    for (Ipp32s x = 0; x < ctbSizeInBlocks; x++)
                        fprintf(fileMv, " (%6.2f, %6.2f)", GetMv(data, size, x, y).x/4.0, GetMv(data, size, x, y).y/4.0);
                    fprintf(fileMv, "\n");
                }
            }
        }
    }

    fclose(fileMv);
    //fclose(fileDist);
}

void H265Encoder::FeiThreadSubmit(ThreadingTask &task)
{
#if TT_TRACE
    char ttname[256] = "Submit ";
    strcat(ttname, GetFeiOpName(task.feiOp));
    strcat(ttname, " ");
    strcat(ttname, NUMBERS[task.poc & 0x7f]);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, ttname);
#endif // TT_TRACE

    assert(task.action == TT_GPU_SUBMIT);
    assert(task.finished == 0);

    mfxFEIH265Input in = {};
    mfxExtFEIH265Output out = {};
	PostProcParam postprocParam;

    in.FrameType = task.frame->m_picCodeType;
    in.FEIOp = task.feiOp;
    in.SaveSyncPoint = 1;

    if (task.feiOp == MFX_FEI_H265_OP_GPU_COPY_SRC) {
        in.copyArgs.surfSys = task.frame->m_origin->m_handle;
        in.copyArgs.surfVid = task.frame->m_feiOrigin->m_handle;
        in.SaveSyncPoint = 0;

    } else if (task.feiOp == MFX_FEI_H265_OP_GPU_COPY_REF) {
        in.copyArgs.surfSys = task.frame->m_recon->m_handle;
        in.copyArgs.surfVid = task.frame->m_feiRecon->m_handle;
        if (m_videoParam.enableCmPostProc == 0)
            in.FEIOp = MFX_FEI_H265_OP_GPU_COPY_SRC;

    } else if (task.feiOp == MFX_FEI_H265_OP_INTRA_MODE) {
        in.meArgs.surfSrc = task.frame->m_feiOrigin->m_handle;
        for (Ipp32s i = 0; i < 4; i++)
            out.SurfIntraMode[i] = task.frame->m_feiIntraAngModes[i]->m_handle;

    } else if (task.feiOp == MFX_FEI_H265_OP_INTER_ME) {
        Frame *ref = task.frame->m_refPicList[task.listIdx].m_refFrames[task.refIdx];
        in.meArgs.surfSrc = task.frame->m_feiOrigin->m_handle;
        in.meArgs.surfRef = ref->m_feiRecon->m_handle;
        in.meArgs.lambda = task.frame->m_slices[0].rd_lambda_sqrt_slice;

        Ipp32s uniqRefIdx = task.frame->m_mapListRefUnique[task.listIdx][task.refIdx];
        for (Ipp32s blksize = 0; blksize < 4; blksize++) {
            Ipp32s feiBlkIdx = blkSizeInternal2Fei[blksize];
            out.SurfInterData[feiBlkIdx] = task.frame->m_feiInterData[uniqRefIdx][blksize]->m_handle;
        }

    } else if (task.feiOp == MFX_FEI_H265_OP_POSTPROC || task.feiOp == MFX_FEI_H265_OP_DEBLOCK) {
        in.postprocArgs.inputSurf = task.frame->m_feiOrigin->m_handle;
        in.postprocArgs.reconSurfSys = task.frame->m_recon->m_handle;
        in.postprocArgs.reconSurfVid = task.frame->m_feiRecon->m_handle;
        in.postprocArgs.cuData = task.frame->m_feiCuData->m_handle;
        in.postprocArgs.saoModes = task.frame->m_feiSaoModes->m_handle;
        in.postprocArgs.param = (mfxU8 *)(&postprocParam);

        Ipp8s *list0_dpoc = task.frame->m_refPicList[0].m_deltaPoc;
        Ipp8s *list1_dpoc = task.frame->m_refPicList[1].m_deltaPoc;
        int list0[33];
        int list1[33];

        for (int dpoc = 0; dpoc < MAX_NUM_ACTIVE_REFS; dpoc++) {
            list0[dpoc] = list0_dpoc[dpoc];
            list1[dpoc] = list1_dpoc[dpoc];
        }

        SetPostProcParam(postprocParam, m_videoParam, list0, list1, task.frame->m_slices[0].rd_lambda_slice);
    } else {
        Throw(std::runtime_error(""));
    }

    mfxFEISyncPoint syncpoint;
    if (H265FEI_ProcessFrameAsync(m_fei, &in, &out, &syncpoint) != MFX_ERR_NONE)
        Throw(std::runtime_error(""));

    //if (task.feiOp == MFX_FEI_H265_OP_INTER_ME) {
    //    H265FEI_SyncOperation(m_fei, syncpoint, 2000);
    //    
    //    DumpGpuMeData(task, m_videoParam);
    //}

    // the following modifications should be guarded by m_feiCritSect
    // because finished, numDownstreamDependencies and downstreamDependencies[]
    // may be accessed from PrepareToEncode()
    vm_mutex_lock(&m_feiCritSect);
    task.finished = 1;

    for (Ipp32s i = 0; i < task.numDownstreamDependencies; i++) {
        ThreadingTask *taskDep = task.downstreamDependencies[i];
        if (vm_interlocked_dec32(&taskDep->numUpstreamDependencies) == 0) {
            if (taskDep->action == TT_GPU_SUBMIT) {
                m_feiSubmitTasks.push_back(taskDep);
            } else if (taskDep->action == TT_GPU_WAIT) {
                assert(taskDep->finished == 0);
                assert(taskDep->syncpoint == NULL);
                assert(in.SaveSyncPoint == 1);
                taskDep->syncpoint = syncpoint;
                m_feiWaitTasks.push_back(taskDep);
            } else {
                Throw(std::runtime_error(""));
            }
        }
    }
    vm_mutex_unlock(&m_feiCritSect);
}

bool H265Encoder::FeiThreadWait(Ipp32u timeout)
{
    ThreadingTask *task = m_feiWaitTasks.front();

#if TT_TRACE
    char ttname[256] = "";
    strcat(ttname, timeout ? "Wait " : "GetStatus ");
    strcat(ttname, GetFeiOpName(task->feiOp));
    strcat(ttname, " ");
    strcat(ttname, NUMBERS[task->poc & 0x7f]);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, ttname);
#endif // TT_TRACE

    mfxStatus sts = H265FEI_SyncOperation(m_fei, task->syncpoint, timeout);
    if (sts == MFX_WRN_IN_EXECUTION)
        return false;
    if (sts != MFX_ERR_NONE)
        Throw(std::runtime_error(""));

    H265FEI_DestroySavedSyncPoint(m_fei, task->syncpoint);
    task->syncpoint = NULL;

    m_feiWaitTasks.pop_front();

    // the following modifications should be guarded by m_critSect
    // because finished, numDownstreamDependencies and downstreamDependencies[]
    // may be accessed from PrepareToEncode() and TaskRoutine()
    vm_mutex_lock(&m_critSect);
    task->finished = 1;
    for (Ipp32s i = 0; i < task->numDownstreamDependencies; i++) {
        ThreadingTask *taskDep = task->downstreamDependencies[i];
        assert(taskDep->action != TT_GPU_SUBMIT && taskDep->action != TT_GPU_WAIT);
        if (vm_interlocked_dec32(&taskDep->numUpstreamDependencies) == 0) {
            m_pendingTasks.push_back(taskDep);
            vm_cond_signal(&m_condVar);
        }
    }
    vm_mutex_unlock(&m_critSect);

    return true;
}


void Hrd::Init(const H265SeqParameterSet &sps, Ipp32u initialDelayInBits)
{
    Ipp32u cpbSize = (sps.cpb_size_value_minus1 + 1) << (4 + sps.cpb_size_scale);
    cbrFlag = sps.cbr_flag;
    bitrate = (sps.bit_rate_value_minus1 + 1) << (6 + sps.bit_rate_scale);
    maxCpbRemovalDelay = 1 << (sps.au_cpb_removal_delay_length_minus1 + 1);

    cpbSize90k = 90000.0 * cpbSize / bitrate;
    initCpbRemovalDelay = 90000.0 * initialDelayInBits / bitrate;
    clockTick = (double)sps.vui_num_units_in_tick / sps.vui_time_scale;

    prevAuCpbRemovalDelayMinus1 = -1;
    prevAuCpbRemovalDelayMsb = 0;
    prevAuFinalArrivalTime = 0.0;
    prevBuffPeriodAuNominalRemovalTime = 0.0;
    prevBuffPeriodEncOrder = 0;
}

void Hrd::Update(Ipp32u sizeInbits, const Frame &pic)
{
    //Ipp32u picDpbOutputDelay = maxNumReorderPics + pic.m_poc - pic.m_encOrder;

    bool bufferingPeriodPic = pic.m_isIdrPic;

    // (C-10)
    double auNominalRemovalTime = initCpbRemovalDelay / 90000;
    if (pic.m_encOrder > 0) {
        Ipp32u auCpbRemovalDelayMinus1 = (pic.m_encOrder - prevBuffPeriodEncOrder) - 1;
        // (D-1)
        Ipp32u auCpbRemovalDelayMsb = 0;
        if (!bufferingPeriodPic)
            auCpbRemovalDelayMsb = ((Ipp32s)auCpbRemovalDelayMinus1 <= prevAuCpbRemovalDelayMinus1)
                ? prevAuCpbRemovalDelayMsb + maxCpbRemovalDelay
                : prevAuCpbRemovalDelayMsb;
        prevAuCpbRemovalDelayMsb = auCpbRemovalDelayMsb;
        prevAuCpbRemovalDelayMinus1 = auCpbRemovalDelayMinus1;
        // (D-2)
        Ipp32u auCpbRemovalDelayValMinus1 = auCpbRemovalDelayMsb + auCpbRemovalDelayMinus1;
        // (C-12)
        auNominalRemovalTime = prevBuffPeriodAuNominalRemovalTime + clockTick * (auCpbRemovalDelayValMinus1 + 1.0);
    }
    // (C-14)
    double auCpbRemovalTime = auNominalRemovalTime;
    // (C-18)
    double deltaTime90k = 90000 * (auNominalRemovalTime - prevAuFinalArrivalTime);

    // update initCpbRemovalDelay (for next picture)
    initCpbRemovalDelay = (cbrFlag)
        // (C-20)
        ? (Ipp32u)(deltaTime90k + 0.5)
        // (C-19)
        : (Ipp32u)MIN(deltaTime90k, cpbSize90k);

    // (C-4)
    double initArrivalTime = prevAuFinalArrivalTime;
    if (!cbrFlag) {
        double initArrivalEarliestTime = (bufferingPeriodPic)
            // (C-8)
            ? auNominalRemovalTime - initCpbRemovalDelay / 90000.0
            // (C-7)
            : auNominalRemovalTime - cpbSize90k / 90000.0;
        // (C-5)
        initArrivalTime = MAX(prevAuFinalArrivalTime, initArrivalEarliestTime);
    }
    // (C-9)
    double auFinalArrivalTime = initArrivalTime + (double)sizeInbits / bitrate;

    prevAuFinalArrivalTime = auFinalArrivalTime;
    if (bufferingPeriodPic) {
        prevBuffPeriodAuNominalRemovalTime = auNominalRemovalTime;
        prevBuffPeriodEncOrder = pic.m_encOrder;
    }
}

Ipp32u Hrd::GetInitCpbRemovalDelay() const
{
    return initCpbRemovalDelay;
}

Ipp32u Hrd::GetInitCpbRemovalOffset() const
{
    return (Ipp32u)cpbSize90k - initCpbRemovalDelay;
}

Ipp32u Hrd::GetAuCpbRemovalDelayMinus1(const Frame &pic) const
{
    return (pic.m_encOrder == 0) ? 0 : (pic.m_encOrder - prevBuffPeriodEncOrder) - 1;
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE2
