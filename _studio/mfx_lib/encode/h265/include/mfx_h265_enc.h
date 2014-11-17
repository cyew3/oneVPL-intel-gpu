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
#include "mfx_ext_buffers.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_paq.h"
#include "mfx_h265_sao_filter.h"
#include "mfx_h265_brc.h"

#ifdef MFX_ENABLE_WATERMARK
class Watermark;
#endif

namespace UMC {
class Semaphore;
}

namespace H265Enc {

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
    Ipp32u QuadtreeTUMaxDepthInterRD;
    Ipp8s  QPI;
    Ipp8s  QPP;
    Ipp8s  QPB;
    Ipp8u  partModes;
    Ipp8u  TMVPFlag;

    Ipp32s NumSlices;
    Ipp32s NumTiles;
    Ipp32s NumTileCols;
    Ipp32s NumTileRows;
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
    Ipp8u  deblockTileBordersFlag;
    Ipp8u  SBHFlag;  // Sign Bit Hiding
    Ipp8u  RDOQFlag; // RDO Quantization
    Ipp8u  FastCoeffCost;   // Use estimator
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
    Ipp32s lookAheadDelay;
    Ipp8u  TryIntra;        // 0-default, 1-always, 2-Try intra based on spatio temporal content analysis in inter
    Ipp8u  FastAMPSkipME;   // 0-default, 1-never, 2-Skip AMP ME of Large Partition when Skip is best
    Ipp8u  FastAMPRD;       // 0-default, 1-never, 2-Adaptive Fast Decision 
    Ipp8u  SkipMotionPartition;  // 0-default, 1-never, 2-Adaptive
    Ipp8u  SkipCandRD;       // on: Do Full RD /off : do Fast decision 
    Ipp8u  BRefSymmetric;    // on: Always symmetric list for B Frames
    Ipp16u cmIntraThreshold;// 0-no theshold
    Ipp16u tuSplitIntra;    // 0-default; 1-always; 2-never; 3-for Intra frames only
    Ipp16u cuSplit;         // 0-default; 1-always; 2-check Skip cost first
    Ipp16u intraAngModes[4];   // Intra Angular modes: [0] I slice, [1] - P, [2] - B Ref, [3] - B non Ref
                               //values for each: 0-default 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99 -DC&Planar only, 100-disable (prohibited for I slice)

    Ipp32s saoOpt;          // 0-all modes; 1-only fast 4 modes
    Ipp16u hadamardMe;      // 0-default; 1-never; 2-subpel; 3-always
    Ipp16u patternIntPel;   // 0-default; 1-log; 3-dia; 100-fullsearch
    Ipp16u patternSubPel;   // 0-default; 1 - int pel only, 2-halfpel; 3-quarter pels; 4-single diamond; 5-double diamond; 6-Fast Box Dia Orth 
    Ipp32s numBiRefineIter;
    Ipp32u num_threads;
    Ipp32u num_thread_structs;
    Ipp32u num_bs_subsets;
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
    Ipp32s m_maxDeltaQP;

    Ipp32u  FrameRateExtN;
    Ipp32u  FrameRateExtD;
    Ipp16u  AspectRatioW;
    Ipp16u  AspectRatioH;
    Ipp16u  Profile;
    Ipp16u  Tier;
    Ipp16u  Level;

    mfxF64 tcDuration90KHz;

    // set
    H265ProfileLevelSet* m_profile_level;
    H265VidParameterSet* m_vps;

    H265SeqParameterSet *csps;
    H265PicParameterSet *cpps;

    Ipp16u tileColStart[MAX_NUM_TILE_COLUMNS];
    Ipp16u tileRowStart[MAX_NUM_TILE_ROWS];
    Ipp16u tileColWidth[MAX_NUM_TILE_COLUMNS];
    Ipp16u tileRowHeight[MAX_NUM_TILE_ROWS];
    Ipp16u tileColWidthMax;
    Ipp16u tileRowHeightMax;

    mfxU8 reconForDump;

    Ipp8u *m_slice_ids;
    Ipp8u *m_tile_ids;
    costStat *m_costStat;
    Ipp8u* m_logMvCostTable;

    // how many frame encoders will be used
    Ipp32s m_framesInParallel; // 0, 1 - default. means no frame threading
    Ipp32s m_meSearchRangeY;   // = Func1 ( m_framesInParallel )
    Ipp32s m_lagBehindRefRows; // = Func2 ( m_framesInParallel ). How many ctb rows in ref frames have to be encoded
    };

class DispatchSaoApplyFilter
{
public:
    DispatchSaoApplyFilter();
    ~DispatchSaoApplyFilter();

    mfxStatus Init(int width, int maxCUWidth, int maxDepth, int bitDepth, int num);
    void Close();
    
    Ipp32u m_bitDepth;
    union {
        SaoDecodeFilter<Ipp8u>* m_sao;
        SaoDecodeFilter<Ipp16u>* m_sao10bit;
    };
};


class H265FrameEncoder : public State {

public:
    H265FrameEncoder();
    ~H265FrameEncoder() { Close(); };
    
    mfxStatus Init(const mfxVideoParam* mfxParam, const H265VideoParam & videoParam, const mfxExtCodingOptionHEVC *optsHevc, VideoCORE *core);
    void      Close();

    // task based API
    mfxStatus SetEncodeTask(Task* task);
    Task* GetEncodeTask() { return m_task;}
    mfxStatus GetOutputData(mfxBitstream *mfxBS, Ipp32s & overheadBytes);

    // frame threading
    void FindJob(Ipp32s & found, Ipp32s & complete, Ipp32u & ctb_row, Ipp32u & ctb_col);
    bool AllReferencesReady(Ipp32u ctb_row);

    template <typename PixType>
    mfxStatus EncodeThread(Ipp32s & ithread, volatile Ipp32u* onExitEvent, UMC::Semaphore &semaphore);

    H265VideoParam* GetVideoParam() {return &m_videoParam;};

private:
    // ------ SAO estimation
    template <typename PixType>
    void EstimateCtuSao(Ipp32s ithread, Ipp32s bs_id, Ipp32s bsf_id, Ipp32u ctb_row, Ipp32u ctb_addr, Ipp8u curr_slice);

    template <typename PixType>
    void PadOneReconRow(Ipp32u ctb_row);

    // SAO Filtering
    template <typename PixType>
    mfxStatus ApplySaoRow(Ipp32u row);

    template <typename PixType>
    mfxStatus ApplySaoCtuRange(Ipp32u ctuStart, Ipp32u ctuEnd);

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
    
    mfxStatus WriteBitstreamPayload(mfxBitstream *mfxBS, Ipp32s bs_main_id, Ipp32s& overheadBytes);
    mfxStatus WriteBitstreamHeaderSet(mfxBitstream *mfxBS, Ipp32s bs_main_id, Ipp32s& overheadBytes, bool isVPSHeader);
    //-----------------------------------------
public: // aya: tmp
    H265BsReal* m_bs;

private:
    H265BsFake* m_bsf;
    Ipp8u *memBuf;
    void *cu;

    std::vector<SaoCtuParam> m_saoParam;

    // const part of encoder
    H265SeqParameterSet m_sps;
    H265PicParameterSet m_pps;
    H265VideoParam m_videoParam;
    Ipp8u *m_logMvCostTable;

    // local data
    Task*      m_task;

    H265CUData *data_temp;
    Ipp32u data_temp_size;

    Ipp8u *m_slice_ids;
    Ipp8u *m_tile_ids;
    costStat *m_costStat;

    CABAC_CONTEXT_H265 *m_context_array_wpp;
    
    DispatchSaoApplyFilter m_saoApplyFilter[NUM_USED_SAO_COMPONENTS];

public:
    volatile Ipp32s m_mt_current_region;
    volatile Ipp32s m_mt_num_regions_done;
    H265EncoderRowInfo* m_row_info;

#ifdef MFX_ENABLE_WATERMARK
    Watermark *m_watermark;
#endif
};

Ipp8s GetConstQp(Task const & task, H265VideoParam const & param);
class H265BRC;
Ipp8s GetRateQp(Task const & task, H265VideoParam const & param, H265BRC* brc);

void SetAllLambda(H265VideoParam const & videoParam, H265Slice *slice, int qp, const H265Frame* currentFrame, bool isHiCmplxGop = false, bool isMidCmplxGop = false);
Ipp64f h265_calc_split_threshold(Ipp32s tabIndex, Ipp32s isNotCu, Ipp32s isNotI, Ipp32s log2width, Ipp32s strength, Ipp32s QP);

} // namespace

#endif // __MFX_H265_ENC_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
