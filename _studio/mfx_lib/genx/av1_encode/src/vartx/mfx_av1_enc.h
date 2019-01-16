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

#ifndef __MFX_H265_ENC_H__
#define __MFX_H265_ENC_H__

#include <deque>
#include "mfx_av1_defs.h"

typedef char vm_char;
enum { MFX_MAX_PATH = 260 };

namespace CodecLimits {
    const int32_t MAX_WIDTH              = 8192;
    const int32_t MAX_HEIGHT             = 4320;
    const int32_t AV1_MAX_NUM_TILE_COLS  = 64;
    const int32_t AV1_MAX_NUM_TILE_ROWS  = 64;
}

namespace H265Enc {

    class H265Encoder;
    class Frame;

    struct H265PriorityRoi {
        uint16_t  left;
        uint16_t  top;
        uint16_t  right;
        uint16_t  bottom;
        int16_t  priority;
    };

    struct AV1SequenceHeader {
        int32_t frameIdNumbersPresentFlag;
        int32_t deltaFrameIdLength;
        int32_t additionalFrameIdLength;
        int32_t idLen;
        int32_t orderHintBits;
        int32_t enableOrderHint;
        int32_t separateUvDeltaQ;
    };

    struct TileParam {
        int32_t uniformSpacing;
        int32_t rows;
        int32_t cols;
        int32_t log2Rows;
        int32_t log2Cols;
        int32_t minLog2Cols;
        int32_t minLog2Rows;
        int32_t maxLog2Cols;
        int32_t maxLog2Rows;
        int32_t minLog2Tiles;
        int32_t maxTileWidthSb;
        int32_t maxTileHeightSb;

        uint32_t mapSb2TileRow[(CodecLimits::MAX_HEIGHT + 63) / 64];
        uint32_t mapSb2TileCol[(CodecLimits::MAX_WIDTH + 63) / 64];

        uint16_t colStart    [CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t colEnd      [CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t colWidth    [CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t rowStart    [CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t rowEnd      [CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t rowHeight   [CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t miColStart  [CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t miColEnd    [CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t miColWidth  [CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t miRowStart  [CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t miRowEnd    [CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t miRowHeight [CodecLimits::AV1_MAX_NUM_TILE_ROWS];
    };

    struct H265VideoParam {
        // preset
        int32_t Log2MaxCUSize;
        int32_t MaxCUDepth;
        int32_t QuadtreeTULog2MaxSize;
        int32_t QuadtreeTULog2MinSize;
        int32_t QuadtreeTUMaxDepthIntra;
        int32_t QuadtreeTUMaxDepthInter;
        int32_t QuadtreeTUMaxDepthInterRD;
        uint8_t  QPI;
        uint8_t  QPP;
        uint8_t  QPB;
        uint8_t  partModes;
        uint8_t  TMVPFlag;
        uint8_t  encodedOrder;

        int32_t NumSlices;
        int32_t NumTiles;
        int32_t NumTileCols;
        int32_t NumTileRows;
        int32_t RegionIdP1;
        uint8_t  AnalyseFlags;
        int32_t GopPicSize;
        int32_t GopRefDist;
        int32_t IdrInterval;
        uint8_t  GopClosedFlag;
        uint8_t  GopStrictFlag;

        int32_t MaxDecPicBuffering;
        int32_t BiPyramidLayers;
        int32_t longGop;
        int32_t MaxRefIdxP[2];
        int32_t MaxRefIdxB[2];
        int32_t GeneralizedPB;

        uint8_t SplitThresholdStrengthCUIntra;
        uint8_t SplitThresholdStrengthTUIntra;
        uint8_t SplitThresholdStrengthCUInter;
        uint8_t SplitThresholdTabIndex;       //0 d3efault (2), 1 - usuallu TU1-3, 2 - TU4-6, 3 fro TU7
        CostType SplitThresholdMultiplier;
        uint8_t num_cand_0[4][8];
        uint8_t num_cand_1[8];
        uint8_t num_cand_2[8];

        uint8_t  deblockingFlag; // Deblocking
        uint8_t  deblockBordersFlag;
        uint8_t  deblockingSharpness;
        uint8_t  deblockingLevelMethod;
        uint8_t  SBHFlag;  // Sign Bit Hiding
        uint8_t  RDOQFlag; // RDO Quantization
        uint8_t  FastCoeffCost;   // Use estimator
        uint8_t  rdoqChromaFlag; // RDOQ Chroma
        uint8_t  rdoqCGZFlag;  // RDOQ Coeff Group Zero
        uint8_t  SAOFlag;      // Sample Adaptive Offset (Luma only)
        uint8_t  SAOChromaFlag;// Sample Adaptive Offset for Chroma
        uint8_t  WPPFlag; // Wavefront
        uint8_t  fastSkip;
        uint8_t  fastCbfMode;
        uint8_t  puDecisionSatd;
        int32_t minCUDepthAdapt;
        int32_t maxCUDepthAdapt;
        uint16_t cuSplitThreshold;

        uint8_t  enableCmFlag;
        uint8_t  enableCmPostProc;  // both: deblock + sao. limitations: 1) 420@8bit (2) sao only for luma
        uint8_t  enablePostProcFrame;
        uint8_t  CmBirefineFlag;  // GPU birefinement
        uint8_t  CmInterpFlag;    // GPU hpel interpolation for every recon

        uint8_t  DeltaQpMode;      // 0 - disable, 0x1 = CAQ, 0x2 = CAL, 0x4 = PAQ
        CostType LambdaCorrection;
        int32_t RateControlDepth; // rate control depth: how many analyzed future frames are required for BRC
        uint8_t  SceneCut;         // Enable Scene Change Detection
        uint8_t  AdaptiveI;        // Enable Scene Change Detection and insert IDR frame
        uint8_t  AnalyzeCmplx;     // analyze frame complexity (for BRC)
        uint8_t  LowresFactor;     // > 0 means lookahead algorithms is applied on downscaled frames
        uint8_t  FullresMetrics;  // 0 use Lowres metrics, 1 means process LA with Lowres but compute final metrics on Fullres

        uint8_t  TryIntra;        // 0-default, 1-always, 2-Try intra based on spatio temporal content analysis in inter
        uint8_t  FastAMPSkipME;   // 0-default, 1-never, 2-Skip AMP ME of Large Partition when Skip is best
        uint8_t  FastAMPRD;       // 0-default, 1-never, 2-Adaptive Fast Decision
        uint8_t  SkipMotionPartition;  // 0-default, 1-never, 2-Adaptive
        uint8_t  SkipCandRD;       // on: Do Full RD /off : do Fast decision
        uint8_t  AdaptiveRefs;     // on: Adaptive Search for best ref
        uint8_t  NumRefFrameB;     // 0,1-default, 2+ Use Given
        uint8_t  NumRefLayers;
        uint8_t  refLayerLimit[4];
        uint8_t  IntraMinDepthSC;  // Spatial complexity for Intra min depth 1 [0-11] 0-default, 1-11 (1-Always, 11-Never)
        uint8_t  InterMinDepthSTC; // Spatio-Temporal complexity for Inter min depth 1 [0-4] 0-default, 1-6 (1-Always, 6-Never)
        uint8_t  MotionPartitionDepth; // MotionPartitionDepth depth [0-6] 0-default, 1-6 (1-Never, 6-Always)
        uint16_t cmIntraThreshold; // 0-no theshold
        uint16_t tuSplitIntra;     // 0-default; 1-always; 2-never; 3-for Intra frames only
        uint16_t cuSplit;          // 0-default; 1-always; 2-check Skip cost first
        uint16_t intraAngModes[4];   // Intra Angular modes: [0] I slice, [1] - P, [2] - B Ref, [3] - B non Ref
        //values for each: 0-default 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99 -DC&Planar only, 100-disable (prohibited for I slice)

        int32_t saoOpt;          // 0-all modes; 1-only fast 4 modes
        int32_t saoSubOpt;       // 0-All; 1-SubOpt; 2-Ref Frames only
        uint16_t hadamardMe;      // 0-default; 1-never; 2-subpel; 3-always
        uint16_t patternIntPel;   // 0-default; 1-log; 3-dia; 100-fullsearch
        uint16_t patternSubPel;   // 0-default; 1 - int pel only, 2-halfpel; 3-quarter pels; 4-single diamond; 5-double diamond; 6-Fast Box Dia Orth
        int32_t numBiRefineIter;
        uint32_t num_threads;
        uint32_t num_thread_structs;
        uint32_t num_bs_subsets;
        uint8_t IntraChromaRDO;   // 1-turns on syntax cost for chroma in Intra
        uint8_t FastInterp;       // 1-turns on fast filters for ME
        uint8_t cpuFeature;       // 0-auto, 1-px, 2-sse4, 3-sse4atom, 4-ssse3, 5-avx2

        uint32_t fourcc;
        uint8_t bitDepthLuma;
        uint8_t bitDepthChroma;
        uint8_t bitDepthLumaShift;
        uint8_t bitDepthChromaShift;

        uint8_t chromaFormatIdc;
        uint8_t chroma422;
        uint8_t chromaShiftW;
        uint8_t chromaShiftH;
        uint8_t chromaShiftWInv;
        uint8_t chromaShiftHInv;
        uint8_t chromaShift;

        // derived
        int32_t PGopPicSize;
        uint32_t sourceWidth;
        uint32_t sourceHeight;
        int32_t Width;
        int32_t Height;
        int32_t picStruct;
        uint32_t CropLeft;
        uint32_t CropTop;
        uint32_t CropRight;
        uint32_t CropBottom;
        int32_t AddCUDepth;
        int32_t MaxCUSize;
        uint32_t MinCUSize; // de facto unused
        int32_t MinTUSize;
        uint32_t MaxTrSize;
        uint32_t Log2NumPartInCU;
        uint32_t NumPartInCU;
        uint32_t NumPartInCUSize;
        uint32_t NumMinTUInMaxCU;
        int32_t PicWidthInCtbs;
        int32_t PicHeightInCtbs;
        int32_t PicWidthInMinCbs;
        int32_t PicHeightInMinCbs;
        CostType cu_split_threshold_cu_sentinel[6*8][2][MAX_TOTAL_DEPTH]; // zero-filled sentinel for those who unintentionally uses qp<0 as index to cu_split_threshold_cu (up to 16bit profiles)
        CostType cu_split_threshold_cu[52][2][MAX_TOTAL_DEPTH];
        CostType cu_split_threshold_tu_sentinel[6*8][2][MAX_TOTAL_DEPTH]; // zero-filled sentinel for those who unintentionally uses qp<0 as index to cu_split_threshold_tu (up to 16bit profiles)
        CostType cu_split_threshold_tu[52][2][MAX_TOTAL_DEPTH];

        // QP control
        uint8_t UseDQP;
        uint32_t MaxCuDQPDepth;
        int32_t MinCuDQPSize;
        int32_t m_maxDeltaQP;

        uint32_t  FrameRateExtN;
        uint32_t  FrameRateExtD;
        uint8_t   hrdPresentFlag;
        uint8_t   cbrFlag;
        uint32_t  targetBitrate;
        uint32_t  hrdBitrate;
        uint32_t  cpbSize;
        uint32_t  initDelay;
        uint16_t  AspectRatioW;
        uint16_t  AspectRatioH;
        uint16_t  Profile;
        uint16_t  Tier;
        uint16_t  Level;
        uint64_t  generalConstraintFlags;
        uint8_t   transquantBypassEnableFlag;
        uint8_t   transformSkipEnabledFlag;
        uint8_t   log2ParallelMergeLevel;
        uint8_t   weightedPredFlag;
        uint8_t   weightedBipredFlag;
        uint8_t   constrainedIntrapredFlag;
        uint8_t   strongIntraSmoothingEnabledFlag;

        double tcDuration90KHz;

        TileParam tileParam;

        uint8_t   doDumpRecon;
        uint8_t   doDumpSource;
        vm_char reconDumpFileName[MFX_MAX_PATH];
        vm_char sourceDumpFileName[MFX_MAX_PATH];
        uint8_t   inputVideoMem;

        // how many frame encoders will be used
        int32_t m_framesInParallel; // 0, 1 - default. means no frame threading
        int32_t m_meSearchRangeY;   // = Func1 ( m_framesInParallel )
        int32_t m_lagBehindRefRows; // = Func2 ( m_framesInParallel ). How many ctb rows in ref frames have to be encoded

        int32_t randomRepackThreshold;

        // priority ROI
        uint16_t numRoi;
        H265PriorityRoi roi[256];

        // VP9 specific
        int32_t subsamplingX;
        int32_t subsamplingY;
        int32_t sb64Cols;
        int32_t sb64Rows;
        int32_t miCols;
        int32_t miRows;
        int32_t miPitch;
        int32_t txMode;
        int32_t interpolationFilter;
        int32_t allowHighPrecisionMv;
        int32_t maxNumReorderPics;
        int32_t errorResilientMode;
        int32_t enableFwdProbUpdateCoef;
        int32_t enableFwdProbUpdateSyntax;
        int32_t enableIntraEdgeFilter;
        int32_t switchableMotionMode;

        int32_t refineModeDecision;
        int32_t maxTxDepthIntra;
        int32_t maxTxDepthInter;
        int32_t maxTxDepthIntraRefine;
        int32_t maxTxDepthInterRefine;
        int32_t chromaRDO;
        int32_t intraRDO;
        int32_t interRDO;
        int32_t intraInterRDO;
        int32_t switchInterpFilter;
        int32_t switchInterpFilterRefine;
        int32_t targetUsage;

        //LoopFilterThresh lfts[8][MAX_LOOP_FILTER + 1];

        QuantParam qparamY[QINDEX_RANGE];
        QuantParam qparamUv[QINDEX_RANGE];

        EnumCodecType codecType;
        AV1SequenceHeader seqParams;
#if defined(DUMP_COSTS_CU) || defined (DUMP_COSTS_TU)
        FILE *fp_cu;
        FILE *fp_tu;
#endif
        uint8_t  cdefFlag; // CDEF
    };

    inline int32_t GetTileIndex(const TileParam &tpar, int32_t ctbRow, int32_t ctbCol)
    {
        const int32_t tileRow = tpar.mapSb2TileRow[ctbRow];
        const int32_t tileCol = tpar.mapSb2TileCol[ctbCol];
        return (tileRow * tpar.cols) + tileCol;
    }

    inline TileBorders GetTileBordersMi(const TileParam &tpar, int32_t miRow, int32_t miCol)
    {
        const int32_t tileCol = tpar.mapSb2TileCol[miCol >> 3];
        const int32_t tileRow = tpar.mapSb2TileRow[miRow >> 3];
        TileBorders borders;
        borders.colStart = tpar.miColStart[tileCol];
        borders.rowStart = tpar.miRowStart[tileRow];
        borders.colEnd   = tpar.miColEnd[tileCol];
        borders.rowEnd   = tpar.miRowEnd[tileRow];
        return borders;
    }

    inline TileBorders GetTileBordersSb(const TileParam &tpar, int32_t sbRow, int32_t sbCol)
    {
        const int32_t tileCol = tpar.mapSb2TileCol[sbCol];
        const int32_t tileRow = tpar.mapSb2TileRow[sbRow];
        TileBorders borders;
        borders.colStart = tpar.colStart[tileCol];
        borders.rowStart = tpar.rowStart[tileRow];
        borders.colEnd   = tpar.colEnd[tileCol];
        borders.rowEnd   = tpar.rowEnd[tileRow];
        return borders;
    }

    void SetupTileParamAv1(H265VideoParam *par, int32_t numTileRows, int32_t numTileCols, int32_t uniform);

    class BrcIface;


    //void SetAllLambda(H265VideoParam const & videoParam, H265Slice *slice, int qp, const Frame* currentFrame, bool isHiCmplxGop = false, bool isMidCmplxGop = false);
    CostType h265_calc_split_threshold(int32_t tabIndex, int32_t isNotCu, int32_t isNotI, int32_t log2width, int32_t strength, int32_t QP);
    void ApplyDeltaQp(Frame* frame, const H265VideoParam & par, uint8_t useBrc = 0);
    //void AddTaskDependency(ThreadingTask *downstream, ThreadingTask *upstream, ObjectPool<ThreadingTask> *ttHubPool = NULL, bool threaded = false);
    //void AddTaskDependencyThreaded(ThreadingTask *downstream, ThreadingTask *upstream, ObjectPool<ThreadingTask> *ttHubPool = NULL);

    bool SliceLambdaMultiplier(CostType &rd_lambda_slice, H265VideoParam const & videoParam, uint8_t frameType, const Frame *currFrame, bool isHiCmplxGop, bool isMidCmplxGop);

    void WriteIvfHeader(uint8_t *buf, int32_t &bitoff, const H265VideoParam &par);
    //void WriteRepeatedFrame(const H265VideoParam &par, const Frame &frame, mfxBitstream &mfxBS);
    int32_t av1_is_interp_needed(const ModeInfo *mi);

} // namespace

#endif // __MFX_H265_ENC_H__
