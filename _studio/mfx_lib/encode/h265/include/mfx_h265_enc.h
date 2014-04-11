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

#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_encode.h"

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

    Ipp32u NumRefFrames;
    Ipp32u NumRefToStartCodeBSlice;
    Ipp8u  TreatBAsReference;
    Ipp8u  BPyramid;
    Ipp8u MaxRefIdxL0;
    Ipp8u MaxRefIdxL1;
    Ipp8u MaxBRefIdxL0;
    Ipp8u GeneralizedPB;

    Ipp8u SplitThresholdStrengthCUIntra;
    Ipp8u SplitThresholdStrengthTUIntra;
    Ipp8u SplitThresholdStrengthCUInter;
    Ipp8u num_cand_0[8];
    Ipp8u num_cand_1[8];
    Ipp8u num_cand_2[8];
    
    Ipp8u  deblockingFlag; // Deblocking
    Ipp8u  SBHFlag;  // Sign Bit Hiding
    Ipp8u  RDOQFlag; // RDO Quantization
    Ipp8u  rdoqChromaFlag; // RDOQ Chroma
    Ipp8u  rdoqCGZFlag; // RDOQ Coeff Group Zero
    Ipp8u  SAOFlag;  // Sample Adaptive Offset
    Ipp8u  WPPFlag; // Wavefront
    Ipp8u  fastPUDecision;
    Ipp8u  fastSkip;
    Ipp8u  fastCbfMode;
    Ipp8u  puDecisionSatd;
    Ipp8u  minCUDepthAdapt;
    Ipp8u  enableCmFlag;
    Ipp16u cmIntraThreshold;// 0-no theshold
    Ipp16u tuSplitIntra;    // 0-default; 1-always; 2-never; 3-for Intra frames only
    Ipp16u cuSplit;         // 0-default; 1-always; 2-check Skip cost first
    Ipp16u intraAngModes;   // 0-default; 1-all; 2-all even + few odd; 3-gradient analysis + few modes
    Ipp32s saoOpt;          // 0-all modes; 1-only fast 4 modes
    Ipp16u hadamardMe;      // 0-default; 1-never; 2-subpel; 3-always
    Ipp16u patternIntPel;   // 0-default; 1-log; 3-dia; 100-fullsearch
    Ipp16u patternSubPel;   // 0-default; 1 - int pel only, 2-halfpel; 3-quarter pels; 4-single diamond; 5-double diamond
    Ipp32u num_threads;
    Ipp32u num_thread_structs;
    Ipp8u threading_by_rows;

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
    Ipp32u MinCUSize;
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
    Ipp32u Log2MinTUSize;
    Ipp8u  AMPAcc[MAX_CU_DEPTH];
    Ipp64f cu_split_threshold_cu[52][2][MAX_TOTAL_DEPTH];
    Ipp64f cu_split_threshold_tu[52][2][MAX_TOTAL_DEPTH];
    Ipp32s MaxTotalDepth;

    Ipp8u UseDQP;
    Ipp32u MaxCuDQPDepth;
    Ipp32u MinCuDQPSize;
    Ipp8s QPIChroma;
    Ipp8s QPPChroma;
    Ipp8s QPBChroma;
    Ipp8s QP;
    Ipp8s QPChroma;

    Ipp32u  FrameRateExtN;
    Ipp32u  FrameRateExtD;
    Ipp16u  AspectRatioW;
    Ipp16u  AspectRatioH;
    Ipp16u  Profile;
    Ipp16u  Tier;
    Ipp16u  Level;

    H265SeqParameterSet *csps;
    H265PicParameterSet *cpps;
    Ipp8u *m_slice_ids;
};

struct H265ShortTermRefPicSet
{
    Ipp8u  inter_ref_pic_set_prediction_flag;
    Ipp32s delta_idx;
    Ipp8u  delta_rps_sign;
    Ipp32s abs_delta_rps;
    Ipp8u  use_delta_flag[MAX_NUM_REF_FRAMES];
    Ipp8u  num_negative_pics;
    Ipp8u  num_positive_pics;
    Ipp32s delta_poc[MAX_NUM_REF_FRAMES]; /* negative+positive */
    Ipp8u  used_by_curr_pic_flag[MAX_NUM_REF_FRAMES]; /* negative+positive */
};

class H265BRC;

class H265Encoder {
public:
    MFXVideoENCODEH265 *mfx_video_encode_h265_ptr;

    H265BsReal *bs;
    H265BsFake *bsf;
    Ipp8u *memBuf;
    Ipp32s m_frameCountEncoded;
    mfxU32 m_frameCountSend;
    H265CU *cu;

    // SAO!!!
    std::vector<SaoCtuParam> m_saoParam;
    SaoDecodeFilter m_saoDecodeFilter;

    H265ProfileLevelSet m_profile_level;
    H265VidParameterSet m_vps;
    H265SeqParameterSet m_sps;
    H265PicParameterSet m_pps;
    H265VideoParam m_videoParam;
    H265Slice *m_slices;
    H265Slice *m_slicesNext;
    H265CUData *data_temp;
    Ipp32u data_temp_size;
    Ipp8u *m_slice_ids;
    H265EncoderRowInfo *m_row_info;

    Ipp32u m_numShortTermRefPicSets;
    Ipp32s m_PicOrderCnt_Accu; // Accumulator to compensate POC resets on IDR frames.
    Ipp32u m_PicOrderCnt;
    Ipp32s m_PGOPIndex;

    H265FrameList m_cpb;
    H265FrameList m_dpb;
    H265Frame *m_pCurrentFrame;
    H265Frame *m_pNextFrame;
    H265Frame *m_pLastFrame;     // ptr to last frame
    H265Frame *m_pReconstructFrame;
    Ipp8u *m_logMvCostTable;

    Ipp32s profile_frequency;
    Ipp32s m_iProfileIndex;
    //Ipp32u *eFrameType;

    //Ipp8u m_bMakeNextFrameKey;
    Ipp8u m_bMakeNextFrameIDR;
    Ipp32s m_uIntraFrameInterval;
    Ipp32s m_uIDRFrameInterval;
    Ipp32s m_l1_cnt_to_start_B;

    Ipp8u m_Bpyramid_nextNumFrame;
    Ipp8u m_Bpyramid_maxNumFrame;
    Ipp8u m_Bpyramid_currentNumFrame;
    Ipp8u m_BpyramidTab[129];
    Ipp8u m_BpyramidTabRight[129];
    Ipp8u m_BpyramidRefLayers[129];
    H265ShortTermRefPicSet m_ShortRefPicSet[66];
    H265ShortTermRefPicSet m_ShortRefPicSetDump[66];

    volatile Ipp32u m_incRow;
    CABAC_CONTEXT_H265 *m_context_array_wpp;

    H265BRC *m_brc;

    const vm_char *m_recon_dump_file_name;

    H265Encoder() {
        memBuf = NULL; bs = NULL; bsf = NULL;
        data_temp = NULL;
        //eFrameType = NULL;
        m_slices = NULL;
        m_slicesNext = NULL;
        m_slice_ids = NULL;
        m_row_info = NULL;
        m_pCurrentFrame = m_pNextFrame = m_pLastFrame = m_pReconstructFrame = NULL;
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
    mfxStatus PutShortTermRefPicSet(H265BsReal *bs_, Ipp32s idx);
    mfxStatus PutSliceHeader(H265BsReal *bs_, H265Slice *slice);

//  SPS, PPS
    mfxStatus SetProfileLevel();
    mfxStatus SetVPS();
    mfxStatus SetSPS();
    mfxStatus SetPPS();
    mfxStatus SetSlice(H265Slice *slice, Ipp32u curr_slice);
    mfxStatus SetSlice(H265Slice *slice, Ipp32u curr_slice, H265Frame *frame);

    void InitShortTermRefPicSet();
    mfxStatus Init(const mfxVideoParam *param, const mfxExtCodingOptionHEVC *opts_hevc);

    void Close();
    Ipp32u    DetermineFrameType();
    void      PrepareToEndSequence();
    mfxStatus EncodeFrame(mfxFrameSurface1 *surface, mfxBitstream *bs);
    mfxStatus DeblockThread(Ipp32s ithread);
    mfxStatus ApplySAOThread(Ipp32s ithread);
    mfxStatus ApplySAOThread_old(Ipp32s ithread); //aya: for debug only
    mfxStatus EncodeThread(Ipp32s ithread);
    mfxStatus EncodeThreadByRow(Ipp32s ithread);
    mfxStatus MoveFromCPBToDPB();
    mfxStatus CleanDPB();

    void CreateRefPicSet(H265Slice *curr_slice);
    mfxStatus CheckCurRefPicSet(H265Slice *curr_slice);
    mfxStatus UpdateRefPicList(H265Slice *curr_slice);
    void CreateRefPicSet(H265Slice *slice, H265Frame *frame);
    mfxStatus CheckRefPicSet(H265Slice *slice, H265Frame *frame);
    mfxStatus UpdateRefPicList(H265Slice *slice, H265Frame *frame);

    mfxStatus InitH265VideoParam(const mfxVideoParam *param, const mfxExtCodingOptionHEVC *opts_hevc);
private:
#ifdef MFX_ENABLE_WATERMARK
    Watermark *m_watermark;
#endif
};

} // namespace

#endif // __MFX_H265_ENC_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
