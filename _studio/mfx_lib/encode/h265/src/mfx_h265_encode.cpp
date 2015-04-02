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
#include <algorithm>

#include "mfxdefs.h"
#include "mfx_common_int.h"
#include "mfx_task.h"
#include "mfx_brc_common.h"
#include "mfx_session.h"
#include "mfx_tools.h"
#include "vm_thread.h"
#include "vm_interlocked.h"
#include "vm_event.h"
#include "vm_cond.h"
#include "vm_sys_info.h"

#include "umc_structures.h"

#include "mfx_ext_buffers.h"

#include "mfx_h265_encode.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_lookahead.h"
#include "mfx_enc_common.h"

#if defined (MFX_VA)
#include "mfx_h265_enc_fei.h"
#endif // MFX_VA

using namespace H265Enc;
using namespace H265Enc::MfxEnumShortAliases;

namespace {
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
        const mfxExtCodingOption2 &opt2 = GetExtBuffer(mfxParam);

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
        intParam.QPI = (Ipp8s)mfx.QPI;
        intParam.QPP = (Ipp8s)mfx.QPP;
        intParam.QPB = (Ipp8s)mfx.QPB;

        intParam.GopPicSize = mfx.GopPicSize;
        intParam.GopRefDist = mfx.GopRefDist;
        intParam.IdrInterval = mfx.IdrInterval;
        intParam.GopClosedFlag = !!(mfx.GopOptFlag & MFX_GOP_CLOSED);
        intParam.GopStrictFlag = !!(mfx.GopOptFlag & MFX_GOP_STRICT);
        intParam.BiPyramidLayers = (optHevc.BPyramid == ON) ? (H265_CeilLog2(intParam.GopRefDist) + 1) : 1;
        intParam.longGop = (optHevc.BPyramid == ON && intParam.GopRefDist == 16 && mfx.NumRefFrame == 5);
        intParam.GeneralizedPB = (optHevc.GPB == ON);
        intParam.AdaptiveRefs = (optHevc.AdaptiveRefs == ON);
        intParam.NumRefFrameB  = optHevc.NumRefFrameB;
        if (intParam.NumRefFrameB < 2) 
            intParam.NumRefFrameB = mfx.NumRefFrame;
        
        intParam.IntraMinDepthSC = (Ipp8u)optHevc.IntraMinDepthSC - 1;
        intParam.InterMinDepthSTC = (Ipp8u)optHevc.InterMinDepthSTC - 1;
        intParam.MotionPartitionDepth = (Ipp8u)optHevc.MotionPartitionDepth - 1;
        intParam.MaxDecPicBuffering = MAX(mfx.NumRefFrame, intParam.BiPyramidLayers);
        intParam.MaxRefIdxP[0] = mfx.NumRefFrame;
        intParam.MaxRefIdxP[1] = intParam.GeneralizedPB ? mfx.NumRefFrame : 0;
        intParam.MaxRefIdxB[0] = 0;
        intParam.MaxRefIdxB[1] = 0;

        if (intParam.GopRefDist > 1) {
            if (intParam.longGop) {
                intParam.MaxRefIdxB[0] = 2;
                intParam.MaxRefIdxB[1] = 2;
            }
            else if (intParam.BiPyramidLayers > 1) {
                Ipp16u NumRef = IPP_MIN(intParam.NumRefFrameB, mfx.NumRefFrame);
                intParam.MaxRefIdxB[0] = (NumRef + 1) / 2;
                intParam.MaxRefIdxB[1] = IPP_MAX(1, (NumRef + 0) / 2);
            } else {
                intParam.MaxRefIdxB[0] = mfx.NumRefFrame - 1;
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

        intParam.LowresFactor = optHevc.LowresFactor;
        intParam.DeltaQpMode = optHevc.DeltaQpMode;
        intParam.SceneCut = optHevc.SceneCut;
        intParam.AnalyzeCmplx = optHevc.AnalyzeCmplx;
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
        intParam.MaxCUSize = 1 << intParam.Log2MaxCUSize;

        intParam.enableCmFlag = (optHevc.EnableCm == ON);
        intParam.m_framesInParallel = optHevc.FramesInParallel;
        // intParam.m_lagBehindRefRows = 3;
        // intParam.m_meSearchRangeY = (intParam.m_lagBehindRefRows - 1) * intParam.MaxCUSize; // -1 row due to deblocking lag
        // New thread model sets up post proc (deblock) as ref task dependency; no deblocking lag, encode is leading.
        intParam.m_lagBehindRefRows = 2;
        intParam.m_meSearchRangeY = intParam.m_lagBehindRefRows * intParam.MaxCUSize;

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

        if (intParam.DeltaQpMode) {
            intParam.MaxCuDQPDepth = 0;
            intParam.m_maxDeltaQP = 0;
            intParam.UseDQP = 1;
        }
        if (intParam.MaxCuDQPDepth > 0 || intParam.m_maxDeltaQP > 0)
            intParam.UseDQP = 1;

        if (intParam.UseDQP)
            intParam.MinCuDQPSize = intParam.MaxCUSize >> intParam.MaxCuDQPDepth;
        else
            intParam.MinCuDQPSize = intParam.MaxCUSize;

        intParam.NumMinTUInMaxCU = intParam.MaxCUSize >> intParam.QuadtreeTULog2MinSize;

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
        intParam.inputVideoMem = (mfxParam.IOPattern == VIDMEM) || (mfxParam.IOPattern == OPAQMEM && opaq.In.Type != MFX_MEMTYPE_SYSTEM_MEMORY);
    }

    struct Deleter { template <typename T> void operator ()(T* p) const { delete p; } };
}; // anonimous namespace


namespace H265Enc {

    struct H265EncodeTaskInputParams
    {
        mfxEncodeCtrl *ctrl;
        mfxFrameSurface1 *surface;
        mfxBitstream *bs;

        // for ParallelFrames > 1
        Ipp32u m_taskID;
        volatile Ipp32u m_doStage;
        Frame* m_targetFrame;      // this frame has to be encoded completely. it is our expected output.
        volatile Ipp32u m_threadCount;
        volatile Ipp32u m_outputQueueSize; // to avoid mutex sync with list::size();
        volatile Ipp32u m_reencode;        // BRC repack

        // FEI
        volatile Ipp32u m_doStageFEI;
    };
};


H265Encoder::H265Encoder(VideoCORE &core)
    : m_core(core)
    , m_brc(NULL)
#if defined (MFX_VA)
    , m_FeiCtx(NULL)
#endif (MFX_VA)
{
    m_videoParam.m_logMvCostTable = NULL;
    m_responseAux.mids = NULL;
    ippStaticInit();
    vm_cond_set_invalid(&m_condVar);
    vm_mutex_set_invalid(&m_critSect);
    Zero(m_stat);
}


H265Encoder::~H265Encoder()
{
    // release resource of frame control
    for_each(m_originPool.begin(), m_originPool.end(), Deleter());
    m_originPool.resize(0);
    for_each(m_originPool_8bit.begin(), m_originPool_8bit.end(), Deleter());
    m_originPool_8bit.resize(0);
    for_each(m_lowresPool.begin(), m_lowresPool.end(), Deleter());
    m_lowresPool.resize(0);

    for_each(m_free.begin(), m_free.end(), Deleter());
    m_free.resize(0);
    for_each(m_inputQueue.begin(), m_inputQueue.end(), Deleter());
    m_inputQueue.resize(0);
    for_each(m_lookaheadQueue.begin(), m_lookaheadQueue.end(), Deleter());
    m_lookaheadQueue.resize(0);
    for_each(m_dpb.begin(), m_dpb.end(), Deleter());
    m_dpb.resize(0);
    // note: m_encodeQueue() & m_outputQueue() only "refer on tasks", not have real ptr. so we don't need to release ones

    for_each(m_statsPool.begin(), m_statsPool.end(), Deleter());
    m_statsPool.resize(0);
    for_each(m_lowresStatsPool.begin(), m_lowresStatsPool.end(), Deleter());
    m_lowresStatsPool.resize(0);
    
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

    if (m_responseAux.mids)
        m_core.FreeFrames(&m_responseAux);

#if defined (MFX_VA)
    if (m_FeiCtx)
        delete m_FeiCtx;
#endif // MFX_VA
    
    m_la.reset(0);

    vm_cond_destroy(&m_condVar);
    vm_mutex_destroy(&m_critSect);

    if (m_memBuf) {
        H265_Free(m_memBuf);
        m_memBuf = NULL;
    }

    if (m_videoParam.SAOFlag) {

        for(Ipp32u idx = 0; idx < m_videoParam.num_thread_structs; idx++) {
            if (m_videoParam.bitDepthLuma == 8) {
                ((H265CU<Ipp8u>*)m_cu)[idx].m_saoEst.Close();
            } else {
                ((H265CU<Ipp16u>*)m_cu)[idx].m_saoEst.Close();
            }
        }
    }
}


mfxStatus H265Encoder::AllocAuxFrame()
{
    mfxFrameAllocRequest request = {};
    request.Info.Width  = (m_videoParam.Width + 15) & ~15;
    request.Info.Height = (m_videoParam.Height + 15) & ~15;
    request.Info.FourCC = m_videoParam.fourcc;
    request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY;
    request.NumFrameMin = 1;
    request.NumFrameSuggested = 1;
    mfxStatus st = m_core.AllocFrames(&request, &m_responseAux);
    if (st != MFX_ERR_NONE)
        return MFX_ERR_MEMORY_ALLOC;
    if (m_responseAux.NumFrameActual < request.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;

    m_auxInput.Data.MemId = m_responseAux.mids[0];
    m_auxInput.Info = request.Info;
    return MFX_ERR_NONE;
}


mfxStatus H265Encoder::Init(const mfxVideoParam &par)
{
    ConvertToInternalParam(m_videoParam, par);

    mfxStatus sts = MFX_ERR_NONE;

    if (m_videoParam.inputVideoMem)
        if (AllocAuxFrame() != MFX_ERR_NONE)
            return MFX_ERR_MEMORY_ALLOC;

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        m_brc = CreateBrc(&par);
        if ((sts = m_brc->Init(&par, m_videoParam)) != MFX_ERR_NONE)
            return sts;
    }

    if ((sts = Init_Internal()) != MFX_ERR_NONE)
        return sts;

    SetProfileLevel();
    SetVPS();
    SetSPS();
    SetPPS();
    SetSeiAps();

    H265BsReal bs;
    bs.m_base.m_pbsBase = m_spsBuf;
    bs.m_base.m_maxBsSize = sizeof(m_spsBuf);
    bs.Reset();
    bs.PutBits(0, 24);
    bs.PutBits((1 << 16) | (NAL_SPS << 9) | 1, 24);
    PutSPS(&bs, m_sps, m_profile_level);
    bs.WriteTrailingBits();
    m_spsBufSize = H265Bs_GetBsSize(&bs);

    bs.m_base.m_pbsBase = m_ppsBuf;
    bs.m_base.m_maxBsSize = sizeof(m_ppsBuf);
    bs.Reset();
    bs.PutBits(0, 24);
    bs.PutBits((1 << 16) | (NAL_PPS << 9) | 1, 24);
    PutPPS(&bs, m_pps);
    bs.WriteTrailingBits();
    m_ppsBufSize = H265Bs_GetBsSize(&bs);

    if (m_videoParam.hrdPresentFlag)
        m_hrd.Init(m_sps, m_videoParam.initDelay);

    // cu
    m_memBuf = NULL;

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


    if (m_videoParam.SAOFlag) {
        for(Ipp32u idx = 0; idx < m_videoParam.num_thread_structs; idx++) {
            if (m_videoParam.bitDepthLuma == 8) {
                ((H265CU<Ipp8u>*)m_cu)[idx].m_saoEst.Init(m_videoParam.Width, m_videoParam.Height,
                    1 << m_videoParam.Log2MaxCUSize, m_videoParam.MaxCUDepth, m_videoParam.bitDepthLuma, 
                    m_videoParam.saoOpt, m_videoParam.SAOChromaFlag, m_videoParam.chromaFormatIdc);
            } else {
                ((H265CU<Ipp16u>*)m_cu)[idx].m_saoEst.Init(m_videoParam.Width, m_videoParam.Height,
                    1 << m_videoParam.Log2MaxCUSize, m_videoParam.MaxCUDepth, m_videoParam.bitDepthLuma,
                    m_videoParam.saoOpt, m_videoParam.SAOChromaFlag, m_videoParam.chromaFormatIdc);
            }
        }
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
    
    if (m_videoParam.SceneCut || m_videoParam.DeltaQpMode || m_videoParam.AnalyzeCmplx)
        m_la.reset(new Lookahead(*this));

    // one more videoParam to simplify logic
    if (m_la.get() && (m_videoParam.LowresFactor || m_videoParam.SceneCut)) {
        m_videoParam_lowres = m_videoParam;
        // hack!!!
        if (m_videoParam_lowres.SceneCut && m_videoParam_lowres.LowresFactor == 0) {
            m_videoParam_lowres.LowresFactor = 1;
        }
        m_videoParam_lowres.Width  >>= m_videoParam_lowres.LowresFactor;
        //m_videoParam_lowres.Width = ((m_videoParam_lowres.Width + 7) >> 3) << 3;
        m_videoParam_lowres.Height >>= m_videoParam_lowres.LowresFactor;
        //m_videoParam_lowres.Height = ((m_videoParam_lowres.Height + 7) >> 3) << 3;
        m_videoParam_lowres.MaxCUSize = SIZE_BLK_LA;
        m_videoParam_lowres.PicWidthInCtbs = (m_videoParam_lowres.Width + m_videoParam_lowres.MaxCUSize - 1) / m_videoParam_lowres.MaxCUSize;
        m_videoParam_lowres.PicHeightInCtbs = (m_videoParam_lowres.Height + m_videoParam_lowres.MaxCUSize - 1) / m_videoParam_lowres.MaxCUSize;
    }

    m_have8bitCopyFlag = (m_videoParam.enableCmFlag && m_videoParam.bitDepthLuma > 8) ? 1 : 0;
    if (m_have8bitCopyFlag) {
        m_videoParam_8bit = m_videoParam;
        m_videoParam_8bit.bitDepthLuma = 8;
        m_videoParam_8bit.bitDepthChroma = 8;
        m_videoParam_8bit.chromaFormatIdc = MFX_CHROMAFORMAT_YUV420;
    }

    m_threadingTaskRunning = 0;
    m_frameCountSync = 0;
    m_frameCountBufferedSync = 0;
    m_taskID = 0;

    m_ithreadPool.resize(m_videoParam.num_thread_structs, 0);
    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::Init_Internal()
{
    Ipp32u memSize = 0;

    memSize += (1 << 16);
    m_videoParam.m_logMvCostTable = (Ipp8u *)H265_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(m_videoParam.m_logMvCostTable);

    // init lookup table for 2*log2(x)+2
    m_videoParam.m_logMvCostTable[(1 << 15)] = 1;
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

    //m_frameCountEncoded = 0;

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

#if defined (MFX_VA)
    if (m_videoParam.enableCmFlag)
        m_FeiCtx = new FeiContext(&m_videoParam, &m_core);
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

mfxStatus H265Encoder::AcceptFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl *ctrl, mfxBitstream *mfxBS)
{
    if (surface) {
        if (m_free.empty()) {
            Frame *newFrame = new Frame;
            newFrame->Create(&m_videoParam);
            m_free.push_back(newFrame);
        }
        m_free.front()->ResetEncInfo();
        m_free.front()->m_timeStamp = surface->Data.TimeStamp;
        m_inputQueue.splice(m_inputQueue.end(), m_free, m_free.begin());
        Frame *inputFrame = m_inputQueue.back();
        assert(inputFrame);

        if (ctrl && ctrl->Payload && ctrl->NumPayload > 0) {
            inputFrame->m_userSeiMessages = ctrl->Payload;
            inputFrame->m_numUserSeiMessages = ctrl->NumPayload;
        }

        if (m_videoParam.encodedOrder) {
            MFX_CHECK_NULL_PTR1(inputFrame);
            MFX_CHECK_NULL_PTR1(ctrl);

            inputFrame->m_picCodeType = ctrl->FrameType;

            if (m_brc && m_brc->IsVMEBRC()) {
                const mfxExtLAFrameStatistics *vmeData = (mfxExtLAFrameStatistics *)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam,MFX_EXTBUFF_LOOKAHEAD_STAT);                
                MFX_CHECK_NULL_PTR1(vmeData);
                mfxStatus sts = m_brc->SetFrameVMEData(vmeData, m_videoParam.Width, m_videoParam.Height);
                MFX_CHECK_STS(sts);
                mfxLAFrameInfo *pInfo = &vmeData->FrameStat[0];  
                inputFrame->m_picCodeType = pInfo->FrameType;
                inputFrame->m_frameOrder = pInfo->FrameDisplayOrder; 
                inputFrame->m_pyramidLayer = pInfo->Layer;
                MFX_CHECK(inputFrame->m_pyramidLayer < m_videoParam.BiPyramidLayers, MFX_ERR_UNDEFINED_BEHAVIOR);
            }

            if (!(inputFrame->m_picCodeType & MFX_FRAMETYPE_B)) {
                m_frameOrderOfLastIdr = m_frameOrderOfLastIdrB;
                m_frameOrderOfLastIntra = m_frameOrderOfLastIntraB;
                m_frameOrderOfLastAnchor = m_frameOrderOfLastAnchorB;
            }
        }

#ifdef MFX_ENABLE_WATERMARK
        // TODO: find appropriate H265FrameEncoder with m_watermark
        // m_watermark->Apply(inputFrame->y, inputFrame->uv, inputFrame->pitch_luma, surface->Info.Width, surface->Info.Height);
        return MFX_ERR_UNSUPPORTED;
#endif

        ConfigureInputFrame(inputFrame, !!m_videoParam.encodedOrder);
        UpdateGopCounters(inputFrame, !!m_videoParam.encodedOrder);

        m_threadingTaskInitNewFrame.Init(TT_INIT_NEW_FRAME, surface, inputFrame, inputFrame->m_frameOrder);
        AddTaskDependency(&m_threadingTaskComplete, &m_threadingTaskInitNewFrame); // INIT_NEW_FRAME -> COMPLETE
        m_pendingTasks.push_back(&m_threadingTaskInitNewFrame);
    } else {
        m_threadingTaskInitNewFrame.indata = NULL;
        m_threadingTaskInitNewFrame.outdata = NULL;
    }
    
    std::list<Frame*> & inputQueue = m_la.get() ? m_lookaheadQueue : m_inputQueue;
    if (m_reorderedQueue.empty()) {
        if (m_videoParam.encodedOrder) {
            if (!inputQueue.empty())   
                m_reorderedQueue.splice(m_reorderedQueue.end(), inputQueue, inputQueue.begin()); 
        } else {
            ReorderFrames(inputQueue, m_reorderedQueue, m_videoParam, surface == NULL);
        }
    }

    if (!m_reorderedQueue.empty()) {
        ConfigureEncodeFrame(m_reorderedQueue.front());
        m_lastEncOrder = m_reorderedQueue.front()->m_encOrder;

        m_dpb.splice(m_dpb.end(), m_reorderedQueue, m_reorderedQueue.begin());
        m_encodeQueue.push_back(m_dpb.back());
    }

    if (!mfxBS)
        return MFX_ERR_NONE;

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::EnqueueFrameEncoder(int& encIdx)
{
    if (m_encodeQueue.empty()) {
        encIdx = -1;
        return MFX_ERR_MORE_DATA;
    }

    for (encIdx = 0; encIdx < (int)m_frameEncoder.size(); encIdx++) {
        if (m_frameEncoder[encIdx]->IsFree()) {
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

    // lasy initialization of frame
    Frame* frame = m_encodeQueue.front();

    frame->m_encIdx = encIdx;

#if defined (MFX_VA)
    if (m_videoParam.enableCmFlag) {
        frame->m_extParam = static_cast<void*>( m_FeiCtx );
    }
#endif

    if ( m_brc && m_videoParam.AnalyzeCmplx > 0 ) {
        Ipp32s framesCount = IPP_MIN((size_t)m_videoParam.RateControlDepth - 1, m_encodeQueue.size()-1);
        frame->m_futureFrames.resize(0);
        FrameIter it = m_encodeQueue.begin();
        it++;
        for (Ipp32s frmIdx = 0; frmIdx < framesCount; frmIdx++ ) {
            frame->m_futureFrames.push_back( (*it) );
            if ( frmIdx+1 < framesCount )
                it++;
        }
    }

    if( m_brc ) {
        frame->m_sliceQpY = GetRateQp(*frame, m_videoParam, m_brc);
    } else {
        frame->m_sliceQpY = GetConstQp(*frame, m_videoParam);
    }

    Ipp32s numCtb = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;
    memset(&frame->m_lcuQps[0], frame->m_sliceQpY, sizeof(frame->m_sliceQpY)*numCtb);
    if (m_videoParam.RegionIdP1 > 0) {
//        memset(task->m_recon->cu_data, 0, sizeof(H265CUData) * (numCtb << m_videoParam.Log2NumPartInCU));
//        memset(task->m_recon->y, 0, m_videoParam.Height * task->m_recon->pitch_luma_bytes);
//        memset(task->m_recon->uv, 0, m_videoParam.Height / 2 * task->m_recon->pitch_chroma_bytes);
        for (Ipp32s i = 0; i < numCtb << m_videoParam.Log2NumPartInCU; i++)
            frame->cu_data[i].predMode = MODE_INTRA;
    }

    // setup slices
    H265Slice *currSlices = &frame->m_slices[0];
    for (Ipp8u i = 0; i < m_videoParam.NumSlices; i++) {
        (currSlices + i)->slice_qp_delta = frame->m_sliceQpY - m_pps.init_qp;
        SetAllLambda(m_videoParam, (currSlices + i), frame->m_sliceQpY, frame );
    }
    
    if (m_videoParam.DeltaQpMode && m_videoParam.UseDQP)
        ApplyDeltaQp(frame, m_videoParam, m_brc ? 1 : 0);
    
    // repack doesn't change downstream dependencies of m_threadingTaskLast
    frame->m_threadingTaskLast.numDownstreamDependencies = 0;
    mfxStatus sts = m_frameEncoder[encIdx]->SetEncodeFrame(frame, &m_pendingTasks);
    MFX_CHECK_STS(sts);
    m_outputQueue.splice(m_outputQueue.end(), m_encodeQueue, m_encodeQueue.begin());

    vm_interlocked_cas32( &(m_outputQueue.back()->m_statusReport), 1, 0);
    //printf("\n submitted encOrder %i \n", task->m_encOrder);fflush(stderr);

    return MFX_ERR_NONE;

} // 


mfxStatus H265Encoder::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, MFX_ENTRY_POINT &entryPoint)
{
    m_frameCountSync++;

    if (!surface) {
        if (m_frameCountBufferedSync == 0) // buffered frames to be encoded
            return MFX_ERR_MORE_DATA;
        m_frameCountBufferedSync--;
    }

    Ipp32s buffering = 0;
    buffering += m_videoParam.GopRefDist - 1;
    buffering += m_videoParam.m_framesInParallel - 1;
    buffering += !!m_videoParam.enableCmFlag;
    if (m_videoParam.SceneCut || m_videoParam.AnalyzeCmplx || m_videoParam.DeltaQpMode) {
        buffering += 1;
        if (m_videoParam.SceneCut)     buffering += 10 + 1 + 1;
        if (m_videoParam.AnalyzeCmplx) buffering += m_videoParam.RateControlDepth;
        if (m_videoParam.DeltaQpMode)  buffering += 2 * m_videoParam.GopRefDist + 1;
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
    m_pTaskInputParams->bs = (status == MFX_ERR_MORE_DATA_RUN_TASK) ? 0 : bs;
    m_pTaskInputParams->ctrl = ctrl;
    m_pTaskInputParams->surface = surface;
    m_pTaskInputParams->m_taskID = m_taskID++;
    m_pTaskInputParams->m_doStage = 0;
    m_pTaskInputParams->m_targetFrame = NULL;
    m_pTaskInputParams->m_threadCount = 0;
    m_pTaskInputParams->m_outputQueueSize = 0;
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

struct isEncodeTask
{
    isEncodeTask(){}

    template<typename T>
    bool operator()(const T& l)
    {
        return ( ((*l).action == TT_ENCODE_CTU) || ((*l).action == TT_POST_PROC_CTU) || ((*l).action == TT_POST_PROC_ROW));
    }
};
mfxStatus H265Encoder::SyncOnFrameCompletion(Frame *frame, mfxBitstream *mfxBs, void *pParam)
{
    H265EncodeTaskInputParams *inputParam = (H265EncodeTaskInputParams *)pParam;
//    Ipp32u statusReport;

    vm_mutex_lock(&m_critSect);
    vm_interlocked_cas32( &(inputParam->m_doStage), 5, 4);
    vm_mutex_unlock(&m_critSect);
    vm_interlocked_cas32( &(frame->m_statusReport), 2, 1);
     
//    if (statusReport == 1)
    {
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
        if (m_brc) {
            const Ipp32s min_qp = 1;
            Ipp32s frameBytes = bs->DataLength - initialDataLength;
            mfxBRCStatus brcSts = m_brc->PostPackFrame(&m_videoParam, frame->m_sliceQpY, frame, frameBytes << 3, overheadBytes << 3, inputParam->m_reencode ? 1 : 0);
//            inputParam->m_reencode = 0;

            if (brcSts != MFX_BRC_OK ) {
                if ((brcSts & MFX_BRC_ERR_SMALL_FRAME) && (frame->m_sliceQpY < min_qp))
                    brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;
                if (brcSts == MFX_BRC_ERR_BIG_FRAME && frame->m_sliceQpY == 51)
                    brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;

                if (!(brcSts & MFX_BRC_NOT_ENOUGH_BUFFER)) {
                    bs->DataLength = dataLength0;
                    H265Bs_SetState(&frameEnc->m_bs[bs_main_id], pbs0, bitOffset0);
                    overheadBytes = overheadBytes0;

                    // [1] wait completion of all running tasks
                    while (m_threadingTaskRunning > 1) thread_sleep(0);

                    FrameIter tit = m_outputQueue.begin();

                    // [2] re-init via BRC
                    // encodeQueue -> outputQueue
                    std::list<Frame*> listQueue[] = {m_outputQueue};//, m_encodeQueue};
                    Ipp32s numCtb = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;
                    Ipp32s qIdxCnt = 1;

                    for (Ipp32s qIdx = 0; qIdx < qIdxCnt; qIdx++) {
                        std::list<Frame*> & queue = listQueue[qIdx];
                        for (tit = queue.begin(); tit != queue.end(); tit++) {
                            
                            (*tit)->m_sliceQpY = GetRateQp( **tit, m_videoParam, m_brc);
                            memset(& (*tit)->m_lcuQps[0], (*tit)->m_sliceQpY, sizeof((*tit)->m_sliceQpY)*numCtb);

                            H265Slice *currSlices = &((*tit)->m_slices[0]);
                            for (Ipp8u i = 0; i < m_videoParam.NumSlices; i++) {
                                (currSlices + i)->slice_qp_delta = (*tit)->m_sliceQpY - m_pps.init_qp;
                                SetAllLambda(m_videoParam, (currSlices + i), (*tit)->m_sliceQpY, (*tit));
                            }

                            if (m_videoParam.DeltaQpMode && m_videoParam.UseDQP) {
                                ApplyDeltaQp(*tit, m_videoParam, 1);
                            }
                        }
                    }

                    // [3] reset encoded data
                    for (FrameIter i = m_outputQueue.begin(); i != m_outputQueue.end(); i++)
                        (*i)->m_codedRow = 0;

                   //m_pendingTasks.clear();
                    m_pendingTasks.erase(std::remove_if(m_pendingTasks.begin(), m_pendingTasks.end(), isEncodeTask()), m_pendingTasks.end());

                    tit = m_outputQueue.begin();
                    for (size_t taskIdx = 0; taskIdx < m_outputQueue.size(); taskIdx++) { // <- issue with Size()!!!

                        if ((*tit)->m_statusReport == 1 || taskIdx == 0) {
                            m_frameEncoder[ (*tit)->m_encIdx ]->SetEncodeFrame( (*tit), &m_pendingTasks );
                        }
                        if (taskIdx + 1 < m_outputQueue.size())
                            tit++;
                    }

                    vm_interlocked_inc32(&inputParam->m_reencode);
                    vm_interlocked_cas32( &(frame->m_statusReport), 1, 2); // signal to restart!!!
                    vm_interlocked_cas32( &(inputParam->m_doStage), 4, 5); // signal to restart!!!

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

        if (m_videoParam.num_threads > 1)
            vm_cond_broadcast(&m_condVar);

        // bs update on completion stage
        vm_interlocked_cas32( &(frame->m_statusReport), 1, 2);
        vm_interlocked_cas32( &(inputParam->m_doStage), 4, 5);

        if (m_videoParam.hrdPresentFlag)
            m_hrd.Update((bs->DataLength - initialDataLength) * 8, *frame);

        MfxAutoMutex guard(m_statMutex);
        m_stat.NumCachedFrame--;
        m_stat.NumFrame++;
        m_stat.NumBit += (bs->DataLength - initialDataLength) * 8;
    }
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

//single sthread only!!!
void H265Encoder::PrepareToEncode(void *pParam)
{
    H265EncodeTaskInputParams *inputParam = (H265EncodeTaskInputParams*)pParam;

    m_threadingTaskComplete.Init(TT_COMPLETE, 0, 0, 0, 0, 0);
    m_threadingTaskInitNewFrame.Init(TT_INIT_NEW_FRAME, (mfxFrameSurface1 *)NULL, (Frame *)NULL, 0);
    m_threadingTaskGpuCurr.Init(TT_GPU, (Frame *)NULL, 0, 0);
    m_threadingTaskGpuNext.Init(TT_GPU, (Frame *)NULL, 0, 0);

    AcceptFrame(inputParam->surface, inputParam->ctrl, inputParam->bs);
    vm_interlocked_cas32(&inputParam->m_doStage, 2, 1);

    if (NULL == inputParam->bs) {
        if (m_la.get()) {
            OnLookaheadStarting();

            if (m_threadingTaskInitNewFrame.outdata && m_threadingTaskInitNewFrame.outdata == m_la->m_frame )
                AddTaskDependency(&m_la->m_threadingTaskStore[0], &m_threadingTaskInitNewFrame); // INIT_NEW_FRAME -> LA_START
            else
                m_pendingTasks.push_back(&m_la->m_threadingTaskStore[0]); // LA_START is independent
        }

        if (m_threadingTaskComplete.numUpstreamDependencies == 0)
            m_pendingTasks.push_front(&m_threadingTaskComplete);

        vm_mutex_lock(&m_critSect);
        vm_interlocked_cas32(&inputParam->m_doStage, 4, 2);
        vm_mutex_unlock(&m_critSect);

        for (int c = 0, taskCount = m_pendingTasks.size(); c < taskCount; c++)
            vm_cond_signal(&m_condVar);

        m_core.INeedMoreThreadsInside(this);
        return;
    }

    Ipp32s stage = vm_interlocked_cas32( &inputParam->m_doStage, 3, 2);
    //if (stage == 2)
    {
        if (m_videoParam.enableCmFlag) {
            Frame *firstToEncode = NULL;
            Frame *nextToEncode = NULL;
            if (!m_outputQueue.empty()) {
                firstToEncode = *m_outputQueue.begin();
                if (m_outputQueue.size() > 1)
                    nextToEncode = *++m_outputQueue.begin();
                else if (!m_encodeQueue.empty())
                    nextToEncode = *m_encodeQueue.begin();
            } else if (!m_encodeQueue.empty()) {
                firstToEncode = *m_encodeQueue.begin();
                if (m_encodeQueue.size() > 1)
                    nextToEncode = *++m_encodeQueue.begin();
            }

            if (firstToEncode) {
                m_threadingTaskGpuCurr.action = TT_GPU;
                m_threadingTaskGpuCurr.frame = firstToEncode;
                m_threadingTaskGpuCurr.isAhead = 0;
                m_threadingTaskGpuCurr.poc = firstToEncode->m_frameOrder;

                if (m_threadingTaskInitNewFrame.outdata && m_threadingTaskInitNewFrame.outdata == m_threadingTaskGpuCurr.frame)
                    AddTaskDependency(&m_threadingTaskGpuCurr, &m_threadingTaskInitNewFrame); // INIT_NEW_FRAME -> GPU(curr)
                if (m_threadingTaskGpuCurr.numUpstreamDependencies == 0)
                    m_pendingTasks.push_back(&m_threadingTaskGpuCurr); // GPU(curr) is independent
            }

            if (nextToEncode) {
                m_threadingTaskGpuNext.action = TT_GPU;
                m_threadingTaskGpuNext.frame = nextToEncode;
                m_threadingTaskGpuNext.isAhead = 1;
                m_threadingTaskGpuNext.poc = nextToEncode->m_frameOrder;

                if (m_threadingTaskGpuCurr.frame)
                    AddTaskDependency(&m_threadingTaskGpuNext, &m_threadingTaskGpuCurr); // GPU(curr) -> GPU(next)
                if (m_threadingTaskInitNewFrame.outdata && m_threadingTaskInitNewFrame.outdata == m_threadingTaskGpuNext.frame)
                    AddTaskDependency(&m_threadingTaskGpuNext, &m_threadingTaskInitNewFrame); // INIT_NEW_FRAME -> GPU(next)
                if (m_threadingTaskGpuNext.numUpstreamDependencies == 0)
                    m_pendingTasks.push_back(&m_threadingTaskGpuNext); // GPU(next) is independent
            }
        }

        while (m_outputQueue.size() < (size_t)m_videoParam.m_framesInParallel && !m_encodeQueue.empty()) {
            int encIdx = 0;
            EnqueueFrameEncoder(encIdx);
        }

        FrameIter it = m_outputQueue.begin();
        inputParam->m_targetFrame    = (*it);
        inputParam->m_outputQueueSize = m_outputQueue.size();

        int finished_ini = inputParam->m_targetFrame->m_threadingTaskLast.finished;
        inputParam->m_targetFrame->m_threadingTaskLast.finished = 0;
        AddTaskDependency(&m_threadingTaskComplete, &(inputParam->m_targetFrame->m_threadingTaskLast)); // ENC_COMPLETE -> COMPLETE
        m_threadingTaskComplete.poc = inputParam->m_targetFrame->m_poc;

        if (m_la.get()) {
            OnLookaheadStarting();
            if (m_threadingTaskInitNewFrame.outdata && m_threadingTaskInitNewFrame.outdata == m_la->m_frame )
                AddTaskDependency(&m_la->m_threadingTaskStore[0], &m_threadingTaskInitNewFrame); // INIT_NEW_FRAME -> LA_START
            else
                m_pendingTasks.push_back(&m_la->m_threadingTaskStore[0]); // LA_START is independent
        }

        if (m_threadingTaskComplete.numUpstreamDependencies == 0) {
            m_pendingTasks.push_front(&m_threadingTaskComplete);
        } else if ((*it)->m_threadingTaskLast.numUpstreamDependencies == 0 && finished_ini == 1) {
            m_pendingTasks.push_front(&(inputParam->m_targetFrame->m_threadingTaskLast));
        }

        vm_mutex_lock(&m_critSect);
        vm_interlocked_cas32( &inputParam->m_doStage, 4, 3);
        vm_mutex_unlock(&m_critSect);

        int c, taskCount = m_pendingTasks.size();
        for (c = 0; c < taskCount; c++)
            vm_cond_signal(&m_condVar);

        m_core.INeedMoreThreadsInside(this);
    }
}

//!!! THREADING - MASTER FUNCTION!!!
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

    // [single thread] :: prepare to encode (accept new frame, configuration, paq etc)
    Ipp32s stage = vm_interlocked_cas32( &inputParam->m_doStage, 1, 0);
    if (0 == stage) {
        th->PrepareToEncode(pParam);// here <m_doStage> will be switched ->2->3->4 consequentially
    }

    // early termination if no external bs 
    //if (NULL == inputParam->bs && th->m_la.get() == 0) {
    //    return MFX_TASK_DONE;
    //} 

    // global thread count control
    Ipp32u newThreadCount = vm_interlocked_inc32( &inputParam->m_threadCount );
    OnExitHelperRoutine onExitHelper( &inputParam->m_threadCount );
    if (newThreadCount > th->m_videoParam.num_threads) {
        return MFX_TASK_BUSY;
    }

    ThreadingTask *taskNext = NULL;
    Ipp32u reencodeCounter = 0;
    while (true) {
        Ipp8u taskNextIsReencodeIndependent = 0;
        if (taskNext && !taskNext->RedoIfRepack()) {
            taskNextIsReencodeIndependent = 1;
        }
        vm_mutex_lock(&th->m_critSect);
        while (1) {
            if (taskNext && !taskNextIsReencodeIndependent && reencodeCounter != inputParam->m_reencode)
                taskNext = NULL;
            if (inputParam->m_doStage == 4 && (taskNext || th->m_pendingTasks.size()))
                break;
            if (inputParam->m_doStage == 5 && taskNext && taskNextIsReencodeIndependent)
                break;
            if (inputParam->m_doStage >= 6)
                break;
            // temp workaround, should be cond_wait. Possibly linux kernel futex bug (not sure yet)
            vm_cond_wait(&th->m_condVar, &th->m_critSect/*, 1*/);
        }
        
        if (inputParam->m_doStage >= 6) {
            if (taskNext) {
                th->m_pendingTasks.push_back(taskNext);
            }
            vm_mutex_unlock(&th->m_critSect);
            break;
        }

        ThreadingTask *task = NULL;
        Ipp8u taskIsReencodeIndependent;
        if (taskNext) {
            task = taskNext;
            taskIsReencodeIndependent = taskNextIsReencodeIndependent;
        } else {
            task = th->m_pendingTasks.front();
            th->m_pendingTasks.pop_front();
            taskIsReencodeIndependent = !task->RedoIfRepack();
        }

        if (!taskIsReencodeIndependent)
            vm_interlocked_inc32(&th->m_threadingTaskRunning);

        taskNext = NULL;
        vm_mutex_unlock(&th->m_critSect);

        Ipp32s taskCount = 0;
        ThreadingTask *taskDepAll[MAX_NUM_DEPENDENCIES];

        Ipp32s distBest;

        sts = MFX_TASK_DONE;
        try {
            switch (task->action) {
            case TT_INIT_NEW_FRAME:
                th->InitNewFrame(task->outdata, task->indata);
                break;
            case TT_GPU:
                th->ProcessFrameOnGpu(task->frame, task->isAhead);
                break;
            case TT_PREENC_START:
            case TT_PREENC_ROUTINE:
            case TT_PREENC_END:
            case TT_HUB:
                sts = task->la->PerformThreadingTask(task->action, task->row, task->col);
                break;
            case TT_ENCODE_CTU:
            case TT_POST_PROC_CTU:
            case TT_POST_PROC_ROW:
                sts = th->m_videoParam.bitDepthLuma > 8 ?
                    task->fenc->PerformThreadingTask<Ipp16u>(task->action, task->row, task->col):
                    task->fenc->PerformThreadingTask<Ipp8u>(task->action, task->row, task->col);
                if (sts != MFX_TASK_DONE) {
                    // shouldn't happen
                    VM_ASSERT_(0);
                }
                break;
            case TT_ENC_COMPLETE:
                if (task->numDownstreamDependencies)
                {
                    sts = th->SyncOnFrameCompletion(inputParam->m_targetFrame, inputParam->bs, inputParam);
                }
                break;
            case TT_COMPLETE:
                vm_mutex_lock(&th->m_critSect);
                vm_interlocked_cas32(&(inputParam->m_doStage), 6, 4);
                vm_mutex_unlock(&th->m_critSect);
                vm_cond_broadcast(&th->m_condVar);
                break;
            default:
                break;
            }
        } catch (...) {
            // to prevent SyncOnFrameCompletion hang
            if (!taskIsReencodeIndependent)
                vm_interlocked_dec32(&th->m_threadingTaskRunning);
            throw;
        }

        if (sts != MFX_TASK_DONE) {
            if (!taskIsReencodeIndependent)
                vm_interlocked_dec32(&th->m_threadingTaskRunning);
            continue;
        }

        task->finished = 1;

        distBest = -1;

        for (int i = 0; i < task->numDownstreamDependencies; i++) {
            ThreadingTask *taskDep = task->downstreamDependencies[i];
            if (vm_interlocked_dec32(&taskDep->numUpstreamDependencies) == 0) {
                if (taskDep->action != TT_COMPLETE &&
                    taskDep->action != TT_ENC_COMPLETE &&
                    taskDep->poc != task->poc) {
                    taskDepAll[taskCount++] = taskDep;
                    continue;
                }

                Ipp32s dist = (abs(task->row - taskDep->row) << 8) + abs(task->col - taskDep->col) + 3;
                if (dist == ((1 << 8) + 1) && task->action == TT_ENCODE_CTU && taskDep->action == TT_POST_PROC_CTU)
                    dist = 2;
                else if (taskDep->action == TT_ENC_COMPLETE)
                    dist = 0;
                else if (taskDep->action == TT_COMPLETE)
                    dist = 1;

                if (distBest < 0 || dist < distBest) {
                    distBest = dist;
                    if (taskNext) {
                        taskDepAll[taskCount++] = taskNext;
                    }
                    taskNext = taskDep;
                } else {                 
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
        if (!taskIsReencodeIndependent)
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
    th->OnLookaheadCompletion();

    H265_Free(pParam);

    return MFX_TASK_DONE;
}


template <class T>
T* H265Enc::Allocate(typename std::vector<T*> & queue, H265VideoParam *par)
{
    for (typename std::vector<T*>::iterator i = queue.begin(); i != queue.end(); ++i)
        if (vm_interlocked_cas32(&(*i)->m_refCounter, 1, 0) == 0)
            return *i;

    std::auto_ptr<T> newFrame(new T());
    newFrame->Create(par);
    newFrame->AddRef();
    queue.push_back(newFrame.release());
    return *(--queue.end());
}

void H265Encoder::InitNewFrame(Frame *out, mfxFrameSurface1 *in)
{
    bool locked = false;
    if (m_videoParam.inputVideoMem) { // copy from d3d to internal frame in system memory
        mfxStatus st = m_core.LockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
        if (st != MFX_ERR_NONE)
            throw std::runtime_error("LockFrame failed");

        st = m_core.DoFastCopyWrapper(&m_auxInput, MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                      in, MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
        if (st != MFX_ERR_NONE)
            throw std::runtime_error("FastCopy failed");

        m_core.DecreaseReference(&in->Data); // do it here
        m_auxInput.Data.FrameOrder = in->Data.FrameOrder;
        m_auxInput.Data.TimeStamp = in->Data.TimeStamp;
        in = &m_auxInput; // replace input pointer
    } else if (in->Data.Y == 0) {
        mfxStatus st = m_core.LockExternalFrame(in->Data.MemId, &in->Data);
        if (st != MFX_ERR_NONE)
            throw std::runtime_error("LockExternalFrame failed");
        locked = true;
    }

    // attach original surface to frame
    out->m_origin = Allocate<FrameData>(m_originPool, &m_videoParam);
    
    // attach 8bit version of original surface to frame
    if (m_have8bitCopyFlag) {
        out->m_luma_8bit = Allocate<FrameData>(m_originPool_8bit, &m_videoParam_8bit);
    }

    // light configure
    out->CopyFrameData(in, m_have8bitCopyFlag);

    // attach lowres surface to frame
    if (m_la.get() && (m_videoParam.LowresFactor || m_videoParam.SceneCut)) {
        out->m_lowres = Allocate<FrameData>(m_lowresPool, &m_videoParam_lowres);
    }

    if (m_videoParam.DeltaQpMode > 0 || m_videoParam.AnalyzeCmplx) {
        FrameData* frame = m_videoParam.LowresFactor ? out->m_lowres : out->m_origin;
        Ipp32s blkSize = m_videoParam.LowresFactor ? SIZE_BLK_LA : m_videoParam.MaxCUSize;
        Ipp32s heightInBlks = (frame->height + blkSize - 1) / blkSize;
        for (Ipp32s row = 0; row < heightInBlks; row++) {
            PadOneReconRow(frame, row, blkSize, heightInBlks, m_videoParam.LowresFactor ? m_videoParam_lowres : m_videoParam);
        }
    }

    // attach statistics to frame
    if (m_la.get()) {
        if (m_videoParam.LowresFactor) {
            out->m_stats[1] = Allocate<Statistics>(m_lowresStatsPool, &m_videoParam_lowres);
            out->m_stats[1]->ResetAvgMetrics();
        }
        if (m_videoParam.DeltaQpMode || m_videoParam.LowresFactor == 0) {
            out->m_stats[0] = Allocate<Statistics>(m_statsPool, &m_videoParam);
            out->m_stats[0]->ResetAvgMetrics();
        }
    }

    // each new frame should be analysed by lookahead algorithms family.
    Ipp32u ownerCount =  (m_videoParam.DeltaQpMode ? 1 : 0) + (m_videoParam.SceneCut ? 1 : 0) + (m_videoParam.AnalyzeCmplx ? 1 : 0);
    out->m_lookaheadRefCounter = ownerCount;

    if (m_videoParam.inputVideoMem) {
        m_core.UnlockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
    } else {
        m_core.DecreaseReference(&in->Data);
        if (locked)
            m_core.UnlockExternalFrame(in->Data.MemId, &in->Data);
    }
}


void H265Encoder::ProcessFrameOnGpu(const Frame *frame_, Ipp32s isAhead)
{
    Frame *frame = const_cast<Frame *>(frame_);
#ifdef MFX_VA
    if (!isAhead) {
        m_FeiCtx->feiInIdx = 1 - m_FeiCtx->feiInIdx;
        m_FeiCtx->ProcessFrameFEI(1 - m_FeiCtx->feiInIdx, frame, frame->m_slices.data(), frame->m_dpb, frame->m_dpbSize, 1);
    } else {
        m_FeiCtx->ProcessFrameFEI(m_FeiCtx->feiInIdx, frame, frame->m_slices.data(), frame->m_dpb, frame->m_dpbSize, 0);
    }
#endif // MFX_VA
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

    prevAuCpbRemovalDelayMinus1 = 0;
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
        prevAuCpbRemovalDelayMinus1 = auCpbRemovalDelayMinus1;
        // (D-1)
        Ipp32u auCpbRemovalDelayMsb = 0;
        if (!bufferingPeriodPic)
            auCpbRemovalDelayMsb = (auCpbRemovalDelayMinus1 <= prevAuCpbRemovalDelayMinus1)
                ? prevAuCpbRemovalDelayMsb + maxCpbRemovalDelay
                : prevAuCpbRemovalDelayMsb;
        prevAuCpbRemovalDelayMsb = auCpbRemovalDelayMsb;
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
