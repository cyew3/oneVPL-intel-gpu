// Copyright (c) 2014-2020 Intel Corporation
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


#pragma once

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include <memory>
#include <list>
#include <vector>
#include <atomic>

#include "vm_interlocked.h"
#include "mfx_av1_defs.h"
#include "mfx_av1_fei.h"
#include "mfx_av1_bit_count.h"
#include "mfx_av1_deblocking.h"
#include "mfx_av1_cdef.h"
#include "mfx_av1_hash_table.h"

#if USE_CMODEL_PAK
#include "av1_encoder_frame.h"
#endif
namespace AV1Enc {

    struct AV1VideoParam;

#ifdef AMT_HROI_PSY_AQ
#define NUM_HROI_CLASS  3
    typedef struct Struct_RoiDataPic {
        float luminanceAvg;
        int32_t numCtbRoi[NUM_HROI_CLASS];
        double cmplx[NUM_HROI_CLASS];
    } RoiDataPic;

    typedef struct Struct_CtbRoiStats {
        int32_t segCount;
        int32_t roiLevel;
        int32_t luminance;
        float spatCmplx;
        float spatMinQCmplx;
        float complexity;
        int32_t sensitivity;
        int32_t sedge;
        int32_t dqp;
    } CtbRoiStats;
#endif

    struct RefCounter
    {
        RefCounter()
            : m_refCounter(0)
        {}
        std::atomic<uint32_t> m_refCounter; // to prevent race condition in case of frame threading
        void AddRef(void) { m_refCounter++; };
        void Release(void) { m_refCounter--; }; // !!! here not to delete in case of refCounter == 0 !!!
    };

    struct Statistics : public RefCounter
    {
        std::vector<int32_t> m_interSad;              // required for AnalyzeCmplx || (DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL))
        std::vector<int32_t> m_interSad_pdist_past;   // required for AnalyzeCmplx || (DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL))
        std::vector<int32_t> m_interSad_pdist_future; // required for AnalyzeCmplx || (DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL))

        std::vector<AV1MV> m_mv;                 // required for AnalyzeCmplx || (DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL))
        std::vector<AV1MV> m_mv_pdist_future;    // required for AnalyzeCmplx || (DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL))
        std::vector<AV1MV> m_mv_pdist_past;      // required for AnalyzeCmplx || (DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL))

        int32_t m_pitchRsCs[5];
        int32_t m_rcscSize[5];
        int32_t* m_rs[5];
        int32_t* m_cs[5];
        int32_t  m_pitchColorCount16x16;
        uint8_t* m_colorCount16x16; // width = framewidth / 16, height = frameheight / 16
        //std::vector<double> rscs_ctb;
        //std::vector<int32_t> sc_mask;
        std::vector<int32_t> qp_mask;    // required for AnalyzeCmplx || (DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL))
        float m_avgDPAQ;
        std::vector<int32_t> coloc_past; // required for AnalyzeCmplx || (DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL))
        std::vector<int32_t> coloc_futr; // required for AnalyzeCmplx || (DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL))

        std::vector<int32_t> m_intraSatd; // required for AnalyzeCmplx
        std::vector<int32_t> m_interSatd; // required for AnalyzeCmplx

#ifdef AMT_HROI_PSY_AQ
        int32_t m_RoiPitch;
        std::vector<uint8_t>  roi_map_8x8;
        std::vector<uint8_t>  lum_avg_8x8;
        RoiDataPic roi_pic;
        CtbRoiStats *ctbStats;
#endif

        double m_frameRs;
        double m_frameCs;
        double SC;
        double nSC;
        double TSC;
        double avgsqrSCpp;
        double avgTSC;
        float m_avgBestSatd; //= sum( min{ interSatd[i], intraSatd[i] } ) / {winth*height};
        float m_avgIntraSatd;
        float m_avgInterSatd;
        float m_intraRatio;
        std::vector<float> m_intraSatdHist;
        std::vector<float> m_bestSatdHist;

        // SceneCut info
        int32_t m_sceneChange;
        int32_t m_sceneCut;
        int64_t m_metric;// special metric per frame for sceneCut based on +10 frames lookahead

        void ResetAvgMetrics()
        {
            m_frameRs = 0.0;
            m_frameCs = 0.0;
            SC = 0.0;
            nSC = 0.0;
            TSC = 0.0;
            avgsqrSCpp = 0.0;
            avgTSC = 0.0;
            m_avgBestSatd = 0.f;
            m_avgIntraSatd = 0.f;
            m_intraRatio = 0.f;
            m_sceneChange = 0;
            m_sceneCut = 0;
            m_metric = 0;
        }

        struct AllocInfo 
        {
            AllocInfo() : width(0), height(0), analyzeCmplx(0), deltaQpMode(0) {}
            int32_t width;
            int32_t height;
            int32_t analyzeCmplx;
            int32_t deltaQpMode;
        };
        Statistics() {
            Zero(m_rs);
            Zero(m_cs);
            m_pitchColorCount16x16 = 0;
            m_colorCount16x16 = nullptr;
            m_avgDPAQ = 0.0f;
#ifdef AMT_HROI_PSY_AQ
            m_RoiPitch = 0;
            ctbStats = NULL;
            roi_pic.luminanceAvg = 128;
            for (int32_t i = 0; i < NUM_HROI_CLASS; i++) {
                roi_pic.numCtbRoi[i] = 0;
                roi_pic.cmplx[i] = 0.0;
            }
#endif
            m_frameRs = 0.0f;
            m_frameCs = 0.0f;
            SC = 0.0f;
            nSC = 0.0f;
            TSC = 0.0f;
            avgsqrSCpp = 0.0f;
            avgTSC = 0.0f;
            m_avgBestSatd = 0.0f;
            m_avgIntraSatd = 0.0f;
            m_avgInterSatd = 0.0f;
            m_intraRatio = 0.0f;
            m_sceneChange = 0;
            m_sceneCut = 0;
            m_metric = 0;
        };
        void Create(const AllocInfo &allocInfo);
        ~Statistics() { Destroy(); }
        void Destroy();
    };

    struct SceneStats : public RefCounter, public NonCopyable
    {
        // padded (+2 pix) Luma
        alignas(16) mfxU8  data[8192]; // aligned pow(2,x) -> extH( 64 + 2*(2)) * extWidth (112 + 2*2)
        mfxU8* Y;

        AV1MV mv[112 /*NUM_BLOCKS*/];
        mfxF32 Cs[448 /*LOWRES_SIZE / 16*/];
        mfxF32 Rs[448 /*LOWRES_SIZE / 16*/];

        mfxF32   avgCs;
        mfxF32   avgRs;
        mfxF32   SC;
        mfxF32   AFD;
        mfxF32   TSC;
        mfxF32   RsCsDiff;
        mfxF32   MVdiffVal;
        mfxU32   RepeatPattern;
#if defined(ENABLE_AV1_ALTR)
        mfxF32   MV0;
        mfxF32 m_ltrMCFD;
        void Create();
#endif
        struct AllocInfo { AllocInfo() : width(0), height(0) {} int32_t width, height; };
        void Create(const AllocInfo &allocInfo);
        void Copy(const SceneStats &);
        ~SceneStats() { Destroy(); }
        void Destroy() {}
    };

    struct FrameData : public RefCounter, public NonCopyable
    {
        uint8_t *y;
        uint8_t *uv;
        mfxHDL m_handle;
        int32_t width;
        int32_t height;
        int32_t padding;
        int32_t pitch_luma_pix;
        int32_t pitch_luma_bytes;
        int32_t pitch_chroma_pix;
        int32_t pitch_chroma_bytes;

        FrameData() 
            : mem()
            , m_fei()
            , y()
            , uv()
            , m_handle()
            , width()
            , height()
            , padding()
            , pitch_luma_pix()
            , pitch_luma_bytes()
            , pitch_chroma_pix()
            , pitch_chroma_bytes()
            {}
        ~FrameData() { Destroy(); }

        struct AllocInfo {
            AllocInfo() { Zero(*this); }
            int32_t width;       // in elements
            int32_t height;      // in elements
            int32_t paddingLu;   // in elements
            int32_t paddingChW;  // in elements
            int32_t paddingChH;  // in elements
            int32_t bitDepthLu;
            int32_t bitDepthCh;
            int32_t chromaFormat;
            // for GACC should be requested from CmDevice
            void  *feiHdl;
            int32_t alignment;
            int32_t pitchInBytesLu;
            int32_t pitchInBytesCh;
            int32_t sizeInBytesLu;
            int32_t sizeInBytesCh;
        };

        void Create(const AllocInfo &allocInfo);
        void Destroy();

    private:
        uint8_t *mem;
        void  *m_fei;
    };

    struct FeiInputData : public RefCounter
    {
        FeiInputData() : m_fei(NULL), m_handle(NULL) {}
        ~FeiInputData() { Destroy(); }

        struct AllocInfo { AllocInfo() : feiHdl(NULL) {} void *feiHdl; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void *m_fei;
        mfxHDL m_handle;
    };

    struct FeiReconData : public RefCounter
    {
        FeiReconData() : m_fei(NULL), m_handle(NULL) {}
        ~FeiReconData() { Destroy(); }

        struct AllocInfo { AllocInfo() : feiHdl(NULL) {} void *feiHdl; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void *m_fei;
        mfxHDL m_handle;
    };

    struct FeiBufferUp : public RefCounter
    {
        FeiBufferUp() : m_fei(), m_handle(), m_allocated(), m_sysmem() {}
        ~FeiBufferUp() { Destroy(); }
        struct AllocInfo { AllocInfo() { Zero(*this); }  mfxHDL feiHdl; uint32_t size, alignment; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void  *m_fei;
        mfxHDL m_handle;
        uint8_t *m_allocated;
        uint8_t *m_sysmem;
    };

    struct FeiOutData : public RefCounter
    {
        FeiOutData() : m_fei(NULL), m_handle(NULL), m_sysmem(NULL), m_pitch(0) {}
        ~FeiOutData() { Destroy(); }
        struct AllocInfo { AllocInfo() { Zero(*this); } void *feiHdl; mfxSurfInfoENC allocInfo; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void *m_fei;
        mfxHDL m_handle;
        mfxU8 *m_sysmem;
        int32_t m_pitch;
    };

    struct FeiSurfaceUp : public RefCounter
    {
        FeiSurfaceUp() : m_fei(NULL), m_handle(NULL), m_sysmem(NULL), m_pitch(0) {}
        ~FeiSurfaceUp() { Destroy(); }
        struct AllocInfo { AllocInfo() { Zero(*this); } void *feiHdl; mfxSurfInfoENC allocInfo; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void *m_fei;
        mfxHDL m_handle;
        std::unique_ptr<mfxU8[]> m_allocated;
        mfxU8 *m_sysmem;
        int32_t m_pitch;
    };


    struct SaoOffsetOut_gfx
    {
        uint8_t mode_idx;
        uint8_t type_idx;
        uint8_t startBand_idx;
        uint8_t saoMaxOffsetQVal;
        int16_t offset[4];
        uint8_t reserved[4];
    }; // sizeof() == 16 bytes

    struct TileContexts {
        TileContexts();
        ~TileContexts() { Free(); }
        void Alloc(int32_t tileSb64Cols, int32_t tileSb64Rows);
        void Free();
        void Clear();
        int32_t tileSb64Cols;
        int32_t tileSb64Rows;
        uint8_t *aboveNonzero[3];
        uint8_t *leftNonzero[3];
        uint8_t *abovePartition;
        uint8_t *leftPartition;
        uint8_t *aboveTxfm;
        uint8_t *leftTxfm;
#ifdef ADAPTIVE_DEADZONE
        ADzCtx **adzctx;
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
        ADzCtx **adzctxDelta;
#endif
#endif
    };


    class AV1FrameEncoder;
    struct ModeInfo;
    struct AV1MvRefs;
    struct PaletteInfo;

    class Frame : public RefCounter
    {
    public:
        int32_t m_frameOrder; // keep it first, good for debug

        AV1FrameEncoder *m_fenc; // valid while frame is being encoded
        FrameData *m_origin;
        FrameData *m_origin10;
        FrameData *m_recon;
        FrameData *m_recon10;
        FrameData *m_reconUpscale;// superres
        FrameData *m_recon10Upscale;// superres
        FrameData *m_lowres; // used by lookahead

        int32_t m_widthLowRes4x;
        int32_t m_heightLowRes4x;
        uint8_t *m_lowres4x; // used by vertical scroll detector
        std::vector<uint32_t> m_vertMvHist;

        ModeInfo *m_modeInfo;

        uint8_t  m_bitDepthLuma;
        uint8_t  m_bdLumaFlag;
        uint8_t  m_bitDepthChroma;
        uint8_t  m_bdChromaFlag;
        uint8_t  m_chromaFormatIdc;

        int32_t m_picStruct;
        int32_t m_secondFieldFlag;
        int32_t m_bottomFieldFlag;
        mfxU64 m_timeStamp;
        uint32_t m_picCodeType;
        int32_t m_RPSIndex;

        // flags indicate that the stages were done on the frame
        uint8_t  m_wasLookAheadProcessed;
        uint32_t m_lookaheadRefCounter;

        int32_t m_pyramidLayer;
        int32_t m_temporalId;
        int32_t m_miniGopCount;
        int32_t m_biFramesInMiniGop;
        int32_t m_frameOrderOfLastIdr;
        int32_t m_frameOrderOfLastIntra;
        int32_t m_frameOrderOfLastBaseTemporalLayer;
        int32_t m_frameOrderOfLastIntraInEncOrder;
        int32_t m_frameOrderOfLastAnchor;
        //int32_t m_frameOrderOfLastLTR;
        int32_t m_frameOrderOfStartSTR;
        int32_t m_frameOrderOfLastExternalLTR;
        int32_t m_poc;
        int32_t m_encOrder;
        uint8_t  m_isShortTermRef;
        uint8_t  m_isIdrPic;
        uint8_t  m_isRef;
        RefPicList m_refPicList[2]; // 2 reflists containing reference frames used by current frame
        int32_t m_mapRefIdxL1ToL0[MAX_NUM_REF_IDX];
        uint32_t m_frameType;// full info for bs. m_frameType = m_codeType | isIDR ? | isRef ?
        int32_t m_sceneOrder;
        int32_t m_temporalSync;
        int32_t m_temporalActv;

        uint8_t m_isLtrFrame;
        uint8_t m_isExternalLTR;
        uint8_t m_shortTermRefFlag;
        uint8_t m_ltrConfidenceLevel;
        int32_t m_ltrDQ;
        mfxExtAVCRefListCtrl refctrl;
        uint8_t m_hasRefCtrl;
        uint8_t m_isErrorResilienceFrame;

        Frame *m_dpb[16];
        Frame *m_dpbVP9[NUM_REF_FRAMES]; // VP9 specific
        Frame *m_dpbVP9NextFrame[NUM_REF_FRAMES]; // VP9 specific
        Frame *m_prevFrame; // VP9 specific: used for PrevMvs & PrevRefFrames, shared ownership
        bool m_prevFrameIsOneOfRefs;
        int32_t m_dpbSize;

        uint8_t m_sliceQpY;
        uint8_t m_sliceQpBrc;
        std::vector<uint8_t> m_lcuQps; // array for LCU QPs
        std::vector<uint8_t> m_lcuRound; // array for LCU Rounding Factors
        uint8_t m_hasMbqpCtrl;
        mfxExtMBQP mbqpctrl;

        // for (frame) threading
        ThreadingTask *m_ttEncode;
        ThreadingTask *m_ttDeblockAndCdef;
        //ThreadingTask  m_ttCdefSync;
        //ThreadingTask  m_ttCdefApply;

        ThreadingTask  m_ttPackHeader;
        ThreadingTask  *m_ttPackTile;
        ThreadingTask  *m_ttPackRow;
        ThreadingTask  m_ttEncComplete;
        ThreadingTask  m_ttInitNewFrame;
        ThreadingTask  m_ttDetectScrollStart;
        ThreadingTask  m_ttDetectScrollRoutine[MAX_NUM_DEPENDENCIES];
        ThreadingTask  m_ttDetectScrollEnd;
        int32_t        m_detectScrollRegionWidth;
        int32_t        m_numDetectScrollRegions;
        ThreadingTask  m_ttSubmitGpuCopySrc;
        ThreadingTask  m_ttSubmitGpuCopyRec;
        ThreadingTask  *m_ttSubmitGpuMe[3];
        ThreadingTask  m_ttWaitGpuCopyRec;
#if PROTOTYPE_GPU_MODE_DECISION
        ThreadingTask  *m_ttSubmitGpuMd;
        ThreadingTask  *m_ttWaitGpuMd;
#else // PROTOTYPE_GPU_MODE_DECISION
        ThreadingTask  m_ttWaitGpuMePu;
#endif // PROTOTYPE_GPU_MODE_DECISION
        ThreadingTask* m_ttLookahead;
        int m_numLaTasks = 0;

        uint32_t m_numThreadingTasks;
#if USE_CMODEL_PAK
        volatile uint32_t m_numFinishedTiles;
#endif

        // complexity statistics
        // 0 - original resolution, 1 - lowres
        Statistics* m_stats[2];
        SceneStats* m_sceneStats;

        // BRC info
        uint32_t m_maxFrameSizeInBitsSet;
        double m_fzCmplx;
        double m_fzCmplxK;
        double m_fzRateE;
        double m_avCmplx;
        double m_CmplxQstep;
        int32_t m_qpBase;
        double m_qsMinForMaxFrameSize;
        int32_t m_qpMinForMaxFrameSize;
        double m_totAvComplx;
        int32_t m_totComplxCnt;
        double m_complxSum;
        int32_t m_predBits;
        double m_cmplx;
        int32_t m_refQp;

        std::vector<Frame *> m_futureFrames;
        uint8_t  m_forceTryIntra;

        // FEI resources
        FeiInputData *m_feiOrigin;
        FeiReconData *m_feiRecon;
        FeiOutData   *m_feiInterData[3][4];
        FeiSurfaceUp *m_feiModeInfo1;
        FeiSurfaceUp *m_feiModeInfo2;
#if GPU_VARTX
        FeiBufferUp  *m_feiAdz;
        FeiBufferUp  *m_feiAdzDelta;
        FeiBufferUp  *m_feiVarTxInfo;
        FeiBufferUp  *m_feiCoefs;
#endif // GPU_VARTX
        FeiSurfaceUp *m_feiRs[4];
        FeiSurfaceUp *m_feiCs[4];

        std::vector<mfxPayload> m_userSeiMessages;
        std::vector<mfxU8>      m_userSeiMessagesData;

        // VP9 specific
        std::vector<Frame *> m_hiddenFrames;
        ThreadingTask  m_ttFrameRepetition;
        uint32_t frameSizeInBytes;
        FrameContexts m_contexts;
        FrameContexts m_contextsInit;
        uint8_t m_refFramesContextsVp9[7][7][4]; // [ aboveRefIdxComb + 2 ] [ leftRefIdxComb + 2 ]
        RefFrameContextsAv1 m_refFramesContextsAv1[6][6]; // [ aboveRefIdxComb + 2 ] [ leftRefIdxComb + 2 ]
        uint16_t m_refFrameBitsAv1[6][6][4];
        int32_t intraOnly;
        int32_t refreshFrameFlags;
        int32_t referenceMode;
        int32_t refFrameIdx[AV1_ALTREF_FRAME + 1];
        Frame *refFramesVp9[ALTREF_FRAME + 1];
        Frame *refFramesAv1[AV1_ALTREF_FRAME + 1];
        int8_t refFrameSide[AV1_ALTREF_FRAME + 1];
        int32_t isUniq[AV1_ALTREF_FRAME+1];
        int32_t refFrameSignBiasVp9[ALTREF_FRAME + 1];
        int32_t refFrameSignBiasAv1[AV1_ALTREF_FRAME+1];
        int32_t m_refFrameId[NUM_REF_FRAMES];
        int32_t interpolationFilter;
        int32_t compoundReferenceAllowed;
        TileParam m_tileParam;
        std::vector<TileContexts> m_tileContexts;
        // contains inital CDFs (common for all tiles)
        // and in case of disable_frame_end_update_cdf=0 contains final CDFs from context_tile_id
        EntropyContexts m_frameEntropyContexts;
        int32_t segmentationEnabled;
        int32_t segmentationUpdateMap;
        int32_t showFrame;
        int32_t showExistingFrame; // 1 for frames which are encoded not in-order, 0 for normal frames
        int32_t frameToShowMapIdx; // frames which are encoded not in-order repeated later
        int32_t compFixedRef; // calculated based on refFrameSignBias
        int32_t compVarRef[2]; // calculated based on refFrameSignBias
        int32_t curFrameOffset;

        CostType m_lambda;
        CostType m_lambdaSatd;
        uint64_t m_lambdaSatdInt; //  = int(m_lambdaSatd * (1<<24))
        const BitCounts *bitCount;

        LoopFilterFrameParam m_loopFilterParam;
        CdefParam m_cdefParam;
        struct SuperResParam { int32_t use = 0; int32_t denom = 0; } m_superResParam;// should be moved to superres header
        int hasTextContent;
        int hasPalettizedContent;
        int m_allowIntraBc;
        int m_allowPalette;

        int16_t m_globalMvy;

        uint32_t isAV1;
        const AV1VideoParam *m_par;

        Frame()
            : bitCount(nullptr)
            , m_recon10Upscale(nullptr)
            , m_widthLowRes4x(0)
            , m_heightLowRes4x(0)
            , m_bitDepthLuma(0)
            , m_bdLumaFlag(0)
            , m_bitDepthChroma(0)
            , m_bdChromaFlag(0)
            , m_chromaFormatIdc(0)
            , m_frameOrderOfStartSTR(MFX_FRAMEORDER_UNKNOWN)
            , m_frameType(0)
            , m_detectScrollRegionWidth(0)
            , m_numDetectScrollRegions(0)
            , m_numThreadingTasks(0)
            , interpolationFilter(0)
            , curFrameOffset(0)
            , m_lambda()
            , m_lambdaSatd()
            , m_lambdaSatdInt(0)
            , hasTextContent(0)
            , hasPalettizedContent(0)
            , m_allowIntraBc(0)
            , m_allowPalette(0)
            , m_globalMvy(0)
            , isAV1(0)
        {
            Zero(m_tileParam);
            Zero(m_cdefParam);
            ResetMemInfo();
            ResetEncInfo();
        }

        ~Frame() { Destroy(); }

        uint8_t wasLAProcessed()    { return m_wasLookAheadProcessed; }
        void setWasLAProcessed() { m_wasLookAheadProcessed = true; }
        void unsetWasLAProcessed() { m_wasLookAheadProcessed = false; }
        int32_t IsIntra() const { return ((m_picCodeType & MFX_FRAMETYPE_I) || intraOnly); }
        int32_t IsNotRef() const { return (!m_isRef); }

        void Create(const AV1VideoParam *par);
        void Destroy();
        void CopyFrameData(const mfxFrameSurface1 *in);

        void ResetMemInfo();
        void ResetEncInfo();
#if USE_CMODEL_PAK
        CmodelAv1::Av1EncoderFrame av1frameOrigin;
        CmodelAv1::Av1EncoderFrame av1frameRecon;
#endif
    };

    typedef std::list<Frame*> FrameList;
    typedef std::list<Frame*>::iterator FrameIter;
    typedef std::list<Frame*>::const_iterator ConstFrameIter;

    void Dump(AV1VideoParam *par, Frame* frame, FrameList & dpb);

    void PadRectLuma         (const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth);
    void PadRectChroma       (const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth);
    void PadRectLumaAndChroma(const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth);

    template <class T> class ObjectPool
    {
    public:
        ObjectPool() {}
        ~ObjectPool() { Destroy(); }

        void Destroy() {
            for (typename std::vector<T *>::iterator i = m_objects.begin(); i != m_objects.end(); ++i)
                delete *i;
            m_objects.resize(0);
        }

        void Init(const typename T::AllocInfo &allocInfo, int32_t numPrealloc)
        {
            assert(m_objects.size() == 0);
            m_allocInfo = allocInfo;

            for (int32_t i = 0; i < numPrealloc; i++)
                Allocate();
            for (typename std::vector<T*>::iterator i = m_objects.begin(); i != m_objects.end(); ++i)
                (*i)->Release();

        }

        const typename T::AllocInfo &GetAllocInfo() const { return m_allocInfo; }

        T *Allocate()
        {
            for (typename std::vector<T*>::iterator i = m_objects.begin(); i != m_objects.end(); ++i) {
                uint32_t exp = 0;
                if ((*i)->m_refCounter.compare_exchange_strong(exp, 1))
                    return *i;
            }

            std::unique_ptr<T> newFrame(new T());
            newFrame->Create(m_allocInfo);
            newFrame->AddRef();
            m_objects.push_back(newFrame.release());
            return *(--m_objects.end());
        }

    protected:
        std::vector<T *> m_objects;
        typename T::AllocInfo m_allocInfo;
    };

    // temp fix, TODO generalize later
    template <> class ObjectPool<ThreadingTask>
    {
    public:
        ObjectPool() {}
        ~ObjectPool() { Destroy(); }
        void Destroy() {
            for (std::vector<ThreadingTask *>::iterator i = m_objects.begin(); i != m_objects.end(); ++i)
                delete *i;
            m_objects.resize(0);
        }
        ThreadingTask *Allocate()
        {
            for (std::vector<ThreadingTask *>::iterator i = m_objects.begin(); i != m_objects.end(); ++i) {
                uint32_t expected = 1;
                if ((*i)->finished.compare_exchange_strong(expected, 0)) {
                    (*i)->action = TT_HUB;
                    (*i)->finished = 0;
                    (*i)->numDownstreamDependencies = 0;
                    (*i)->numUpstreamDependencies = 0;
                    return *i;
                }
            }
            std::unique_ptr<ThreadingTask> newHub(new ThreadingTask());
            newHub->action = TT_HUB;
            newHub->finished = 0;
            newHub->numDownstreamDependencies = 0;
            newHub->numUpstreamDependencies = 0;
            m_objects.push_back(newHub.release());
            return *(--m_objects.end());
        }
        void ReleaseAll()
        {
            for (std::vector<ThreadingTask *>::iterator i = m_objects.begin(); i != m_objects.end(); ++i)
                (*i)->finished = 1;
        }
    protected:
        std::vector<ThreadingTask *> m_objects;
    };


    template <class T> void SafeRelease(T *&obj)
    {
        if (obj) {
            obj->Release();
            obj = NULL;
        }
    }

} // namespace

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
