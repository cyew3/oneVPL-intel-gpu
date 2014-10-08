//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_ENC_H__
#define __MFX_H265_ENC_H__

#include <list>
#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_encode.h"
#include "mfx_h265_paq.h"
#define  MAX_DQP (6)

#ifdef MFX_ENABLE_WATERMARK
class Watermark;
#endif

namespace H265Enc {

enum ParallelRegion
{
    PARALLEL_REGION_MAIN,
    PARALLEL_REGION_DEBLOCKING
};

struct H265VideoParam {
// preset
    Ipp32u SourceWidth;
    Ipp32u SourceHeight;
    Ipp32u Log2MaxCUSize;
    Ipp32s MaxCUDepth;
    Ipp32u QuadtreeTULog2MaxSize;
    Ipp32u QuadtreeTULog2MinSize;
    Ipp32u QuadtreeTUMaxDepthIntra;
    Ipp32u QuadtreeTUMaxDepthInter;
    Ipp8s  QPI;
    Ipp8s  QPP;
    Ipp8s  QPB;
    Ipp8u  partModes;
    Ipp8u  TMVPFlag;

    Ipp32s NumSlices;
    Ipp8u  AnalyseFlags;
    Ipp32s GopPicSize;
    Ipp32s GopRefDist;
    Ipp32s IdrInterval;
    Ipp8u  GopClosedFlag;
    Ipp8u  GopStrictFlag;

    Ipp32s MaxDecPicBuffering;
    Ipp32s NumRefToStartCodeBSlice;
    Ipp8u  TreatBAsReference;
    Ipp32s BiPyramidLayers;
    Ipp32s longGop;
    Ipp32s MaxRefIdxP[2];
    Ipp32s MaxRefIdxB[2];
    Ipp32s GeneralizedPB;

    Ipp8u SplitThresholdStrengthCUIntra;
    Ipp8u SplitThresholdStrengthTUIntra;
    Ipp8u SplitThresholdStrengthCUInter;
    Ipp8u SplitThresholdTabIndex;       //0 d3efault (2), 1 - usuallu TU1-3, 2 - TU4-6, 3 fro TU7
    Ipp8u num_cand_0[4][8];
    Ipp8u num_cand_1[8];
    Ipp8u num_cand_2[8];
    
    Ipp8u  deblockingFlag; // Deblocking
    Ipp8u  SBHFlag;  // Sign Bit Hiding
    Ipp8u  RDOQFlag; // RDO Quantization
    Ipp8u  rdoqChromaFlag; // RDOQ Chroma
    Ipp8u  rdoqCGZFlag; // RDOQ Coeff Group Zero
    Ipp8u  SAOFlag;  // Sample Adaptive Offset
    Ipp8u  WPPFlag; // Wavefront
    Ipp8u  fastSkip;
    Ipp8u  fastCbfMode;
    Ipp8u  puDecisionSatd;
    Ipp8u  minCUDepthAdapt;
    Ipp8u  maxCUDepthAdapt;
    Ipp16u cuSplitThreshold;
    Ipp8u  enableCmFlag;
    Ipp8u  preEncMode; // pre Encode Analysis
    Ipp8u  TryIntra;        // 0-default, 1-always, 2-Try intra based on spatio temporal content analysis in inter
    Ipp8u  FastAMPSkipME;   // 0-default, 1-never, 2-Skip AMP ME of Large Partition when Skip is best
    Ipp8u  FastAMPRD;       // 0-default, 1-never, 2-Adaptive Fast Decision 
    Ipp8u  SkipMotionPartition;  // 0-default, 1-never, 2-Adaptive
    Ipp8u  SkipCandRD;       // on: Do Full RD /off : do Fast decision 
    Ipp16u cmIntraThreshold;// 0-no theshold
    Ipp16u tuSplitIntra;    // 0-default; 1-always; 2-never; 3-for Intra frames only
    Ipp16u cuSplit;         // 0-default; 1-always; 2-check Skip cost first
    Ipp16u intraAngModes[4];   // Intra Angular modes: [0] I slice, [1] - P, [2] - B Ref, [3] - B non Ref
                               //values for each: 0-default 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99 -DC&Planar only, 100-disable (prohibited for I slice)

    Ipp32s saoOpt;          // 0-all modes; 1-only fast 4 modes
    Ipp16u hadamardMe;      // 0-default; 1-never; 2-subpel; 3-always
    Ipp16u patternIntPel;   // 0-default; 1-log; 3-dia; 100-fullsearch
    Ipp16u patternSubPel;   // 0-default; 1 - int pel only, 2-halfpel; 3-quarter pels; 4-single diamond; 5-double diamond
    Ipp32s numBiRefineIter;
    Ipp32u num_threads;
    Ipp32u num_thread_structs;
    Ipp8u threading_by_rows;
    Ipp8u IntraChromaRDO;   // 1-turns on syntax cost for chroma in Intra
    Ipp8u FastInterp;       // 1-turns on fast filters for ME
    Ipp8u cpuFeature;       // 0-auto, 1-px, 2-sse4, 3-sse4atom, 4-ssse3, 5-avx2

    Ipp8u bitDepthLuma;
    Ipp8u bitDepthChroma;
    Ipp8u bitDepthLumaShift;
    Ipp8u bitDepthChromaShift;

    Ipp8u chromaFormatIdc;
    Ipp8u chroma422;
    Ipp8u chromaShiftW;
    Ipp8u chromaShiftH;
    Ipp8u chromaShiftWInv;
    Ipp8u chromaShiftHInv;
    Ipp8u chromaShift;

// derived
    Ipp32s PGopPicSize;
    Ipp32u Width;
    Ipp32u Height;
    Ipp32u CropLeft;
    Ipp32u CropTop;
    Ipp32u CropRight;
    Ipp32u CropBottom;
    Ipp32s AddCUDepth;
    Ipp32u MaxCUSize;
    Ipp32u MinCUSize; // de facto unused
    Ipp32u MinTUSize;
    Ipp32u MaxTrSize;
    Ipp32u Log2NumPartInCU;
    Ipp32u NumPartInCU;
    Ipp32u NumPartInCUSize;
    Ipp32u NumMinTUInMaxCU;
    Ipp32u PicWidthInCtbs;
    Ipp32u PicHeightInCtbs;
    Ipp32u PicWidthInMinCbs;
    Ipp32u PicHeightInMinCbs;
    //Ipp32u Log2MinTUSize; // duplicated QuadtreeTULog2MinSize
    Ipp8u  AMPAcc[MAX_CU_DEPTH];
    Ipp64f cu_split_threshold_cu[52][2][MAX_TOTAL_DEPTH];
    Ipp64f cu_split_threshold_tu[52][2][MAX_TOTAL_DEPTH];
    //Ipp32s MaxTotalDepth; // duplicated MaxCUDepth

    // QP control
    Ipp8u UseDQP;
    Ipp32u MaxCuDQPDepth;
    Ipp32u MinCuDQPSize; 
    int m_maxDeltaQP;
    Ipp8s* m_lcuQps; // array for LCU QPs

    //use slice QP_Y for the frame level math and m_lcuQps[ctbAddr] for the CTB level math.
    Ipp8s m_sliceQpY; // npshosta: WTF? find better place for this

    Ipp32u  FrameRateExtN;
    Ipp32u  FrameRateExtD;
    Ipp16u  AspectRatioW;
    Ipp16u  AspectRatioH;
    Ipp16u  Profile;
    Ipp16u  Tier;
    Ipp16u  Level;

    const H265SeqParameterSet *csps;
    const H265PicParameterSet *cpps;
    Ipp8u *m_slice_ids;
    costStat *m_costStat; // npshosta: find more appropriate place for this
    H265Slice m_dqpSlice[2*MAX_DQP+1]; // npshosta: find more appropriate place for this
};


class H265BRC;
#if defined(MFX_VA)
class FeiContext;
#endif

class H265Encoder {

public:

    MFXVideoENCODEH265 *mfx_video_encode_h265_ptr;

    H265BsReal* m_bs;
    H265BsFake* m_bsf;
    Ipp8u *memBuf;
    Ipp32s m_frameCountEncoded;
    void *cu;

    std::vector<SaoCtuParam> m_saoParam;

    H265ProfileLevelSet m_profile_level;
    H265VidParameterSet m_vps;
    H265SeqParameterSet m_sps;
    H265PicParameterSet m_pps;
    H265VideoParam m_videoParam;

    // local data
    H265Slice *m_slices;
    H265Slice *m_slicesNext;

    // what is it??? it is temporary CU Data for core encoder
    H265CUData *data_temp;
    Ipp32u data_temp_size;

    Ipp8u *m_slice_ids;
    H265EncoderRowInfo *m_row_info;
    costStat *m_costStat;

    // input frame control counters
    Ipp32s m_profileIndex;
    Ipp32s m_frameOrder;
    Ipp32s m_frameOrderOfLastIdr;       // frame order of last IDR frame (in display order)
    Ipp32s m_frameOrderOfLastIntra;     // frame order of last I-frame (in display order)
    Ipp32s m_frameOrderOfLastAnchor;    // frame order of last anchor (first in minigop) frame (in display order)
    Ipp32s m_miniGopCount;
    mfxU64 m_lastTimeStamp;
    Ipp32s m_lastEncOrder;
    
    // input frame control queues
    std::list<Task*> m_free;             // _global_ free task pool
    std::list<Task*> m_inputQueue_1;     // _global_ input task queue in _display_ order
    std::list<Task*> m_reorderedQueue_1; // _global_ input task queue in _display_ order
    std::list<Task*> m_dpb_1;            // _global_ pool of frames: encoded reference frames + being encoded frames
    std::list<Task*> m_lookaheadQueue;   // _global_ queue in _encode_ order
    std::list<Task*> m_encodeQueue_1;    // _global_ queue in _encode_ order (contains _ptr_ to encode task)
    std::list<Task*> m_actualDpb;        // reference frames for next frame to encode
    std::list<H265Frame*> m_freeFrames;  // _global_ free frames (origin/recon/reference) pool

    H265Frame *m_pCurrentFrame;
    H265Frame *m_pReconstructFrame;
    H265Frame *m_pNextFrame; // for GACC
    
    Ipp8u *m_logMvCostTable;

    volatile Ipp32u m_incRow;
    CABAC_CONTEXT_H265 *m_context_array_wpp;

    H265BRC *m_brc;
    TAdapQP *m_pAQP;

#if defined(MFX_ENABLE_H265_PAQ)
    TVideoPreanalyzer m_preEnc;
#endif

#if defined(MFX_VA)
    FeiContext *m_FeiCtx;
#endif

    const vm_char *m_recon_dump_file_name;

    H265Encoder() {
        memBuf = NULL; m_bs = NULL; m_bsf = NULL;
        data_temp = NULL;
        m_slices = NULL;
        m_slice_ids = NULL;
        m_row_info = NULL;

        m_pCurrentFrame = m_pNextFrame = m_pReconstructFrame = NULL;

        /*m_lastTask.m_frameOrigin = NULL;
        m_lastTask.m_frameRecon = NULL;*/
        m_brc = NULL;
        m_context_array_wpp = NULL;
        m_recon_dump_file_name = NULL;
#ifdef MFX_ENABLE_WATERMARK
        m_watermark = NULL;
#endif
    }
    ~H265Encoder() {};
///////////////
    // ------ bitstream
    mfxStatus WriteEndOfSequence(mfxBitstream *bs);
    mfxStatus WriteEndOfStream(mfxBitstream *bs);
    mfxStatus WriteFillerData(mfxBitstream *bs, mfxI32 num);
    void PutProfileLevel(H265BsReal *bs_, Ipp8u profile_present_flag, Ipp32s max_sub_layers);
    mfxStatus PutVPS(H265BsReal *bs_);
    mfxStatus PutSPS(H265BsReal *bs_);
    mfxStatus PutPPS(H265BsReal *bs_);
    mfxStatus PutShortTermRefPicSet(H265BsReal *bs_, const H265ShortTermRefPicSet *rps, Ipp32s idx);
    mfxStatus PutSliceHeader(H265BsReal *bs_, H265Slice *slice);

    mfxStatus WriteBitstream(mfxBitstream *mfxBS, bool isFirstTime);
    mfxStatus WriteBitstream_Internal(mfxBitstream *mfxBS, H265NALUnit& nal, Ipp32s bs_main_id, Ipp32s& overheadBytes);

   // ------SPS, PPS
    mfxStatus SetProfileLevel();
    mfxStatus SetVPS();
    mfxStatus SetSPS();
    mfxStatus SetPPS();
    mfxStatus SetSlice(H265Slice *slice, Ipp32u curr_slice, H265Frame* currentFrame);
    void SetAllLambda(H265Slice *slice, Ipp32s qp, const H265Frame *currFrame, bool isHiCmplxGop = false, bool isMidCmplxGop = false);

    mfxStatus Init(const mfxVideoParam *param, const mfxExtCodingOptionHEVC *opts_hevc, VideoCORE *core);
    void      Close();
    mfxStatus InitH265VideoParam(const mfxVideoParam *param, const mfxExtCodingOptionHEVC *opts_hevc);    

    // ------ _global_ stages of Input Frame Control
    H265Frame *InsertInputFrame(const mfxFrameSurface1 *surface);
    void ConfigureInputFrame(H265Frame *frame) const;
    void PrepareToEncode(Task *task);
    void OnEncodingSubmitted(TaskIter task);
    void OnEncodingQueried(Task *task);
    void UpdateDpb(Task *currTask);
    void CleanTaskPool();
    void CreateRefPicList(Task *task, H265ShortTermRefPicSet *rps);

    // ------ Ref List Managment
    void InitShortTermRefPicSet();
    // ----------------------------------------------------

    // ------ core encode routines
    mfxStatus EncodeFrame(mfxFrameSurface1 *surface, mfxBitstream *bs);
    template <typename PixType>
    mfxStatus DeblockThread(Ipp32s ithread);
    template <typename PixType>    
    mfxStatus ApplySAOThread(Ipp32s ithread);
    mfxStatus ApplySAOThread_old(Ipp32s ithread); //aya: for debug only
    template <typename PixType>    
    mfxStatus EncodeThread(Ipp32s ithread);
    template <typename PixType>    
    mfxStatus EncodeThreadByRow(Ipp32s ithread);    

    mfxStatus PreEncAnalysis(mfxBitstream* mfxBS);
    mfxStatus UpdateAllLambda(const H265Frame* frame);// current frame to encode. POC & B frame pos are needed only

private:
#ifdef MFX_ENABLE_WATERMARK
    Watermark *m_watermark;
#endif
};

} // namespace

#endif // __MFX_H265_ENC_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
