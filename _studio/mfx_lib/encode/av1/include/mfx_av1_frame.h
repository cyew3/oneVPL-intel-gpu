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


#pragma once

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include <memory>
#include <list>
#include <vector>
#include "vm_interlocked.h"
#include "mfx_av1_defs.h"
#include "mfx_av1_fei.h"
#include "mfx_av1_bit_count.h"
#include "mfx_av1_deblocking.h"
#include "mfx_av1_cdef.h"

#if USE_CMODEL_PAK
#include "av1_encoder_frame.h"
#endif
namespace AV1Enc {

    struct AV1VideoParam;

    struct RefCounter
    {
        RefCounter()
            : m_refCounter(0)
        {}
        volatile uint32_t m_refCounter; // to prevent race condition in case of frame threading
        void AddRef(void)  { vm_interlocked_inc32(&m_refCounter);};
        void Release(void) { vm_interlocked_dec32(&m_refCounter);}; // !!! here not to delete in case of refCounter == 0 !!!
    };

    struct Statistics : public RefCounter
    {
        std::vector<int32_t> m_interSad;
        std::vector<int32_t> m_interSad_pdist_past;
        std::vector<int32_t> m_interSad_pdist_future;

        std::vector<AV1MV> m_mv;
        std::vector<AV1MV> m_mv_pdist_future;
        std::vector<AV1MV> m_mv_pdist_past;

        int32_t m_pitchRsCs4;
        int32_t m_rcscSize[5];
        int32_t* m_rs[5];
        int32_t* m_cs[5];
        std::vector<double> rscs_ctb;
        std::vector<int32_t> sc_mask;
        std::vector<int32_t> qp_mask;
        std::vector<int32_t> coloc_past;
        std::vector<int32_t> coloc_futr;

        std::vector<int32_t> m_intraSatd;
        std::vector<int32_t> m_interSatd;

        double m_frameRs;
        double m_frameCs;
        double SC;
        double TSC;
        double avgsqrSCpp;
        double avgTSC;
        float m_avgBestSatd; //= sum( min{ interSatd[i], intraSatd[i] } ) / {winth*height};
        float m_avgIntraSatd;
        float m_avgInterSatd;
        float m_intraRatio;

        // SceneCut info
        int32_t m_sceneCut;
        int64_t m_metric;// special metric per frame for sceneCut based on +10 frames lookahead

        void ResetAvgMetrics()
        {
            m_frameRs = 0.0;
            m_frameCs = 0.0;
            SC = 0.0;
            TSC = 0.0;
            avgsqrSCpp = 0.0;
            avgTSC = 0.0;
            m_avgBestSatd = 0.f;
            m_avgIntraSatd = 0.f;
            m_intraRatio = 0.f;
            m_sceneCut = 0;
            m_metric    = 0;
        }

        struct AllocInfo { AllocInfo() : width(0), height(0) {} int32_t width, height; };
        Statistics() { Zero(m_rs); Zero(m_cs); };
        void Create(const AllocInfo &allocInfo);
        ~Statistics() { Destroy(); }
        void Destroy();
    };

    struct SceneStats : public RefCounter, public NonCopyable
    {
        // padded (+2 pix) Luma
        alignas(16) mfxU8  data[8192]; // aligned pow(2,x) -> extH( 64 + 2*(2)) * extWidth (112 + 2*2)
        mfxU8* Y;

        AV1MV mv[112 /*NUM_BLOCKS*/       ];
        mfxF32 Cs[448 /*LOWRES_SIZE / 16*/ ];
        mfxF32 Rs[448 /*LOWRES_SIZE / 16*/ ];

        mfxF32   avgCs;
        mfxF32   avgRs;

        mfxF32   AFD;
        mfxF32   TSC;
        mfxF32   RsCsDiff;
        mfxF32   MVdiffVal;

        struct AllocInfo { AllocInfo() : width(0), height(0) {} int32_t width, height; };
        void Create(const AllocInfo &allocInfo);
        ~SceneStats() { Destroy(); }
        void Destroy(){}
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

        FrameData() : y(), uv(), m_handle(), mem(), m_fei() {}
        ~FrameData() { Destroy(); }

        struct AllocInfo {
            AllocInfo()  { Zero(*this); }
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
        struct AllocInfo { AllocInfo() {Zero(*this);}  mfxHDL feiHdl; uint32_t size, alignment; };
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
        struct AllocInfo { AllocInfo() {Zero(*this);} void *feiHdl; mfxSurfInfoENC allocInfo; };
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
        struct AllocInfo { AllocInfo() {Zero(*this);} void *feiHdl; mfxSurfInfoENC allocInfo; };
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
        EntropyContexts ectx;
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

    class Frame : public RefCounter
    {
    public:
        int32_t m_frameOrder; // keep it first, good for debug

        AV1FrameEncoder *m_fenc; // valid while frame is being encoded
        FrameData *m_origin;
        FrameData *m_origin10;
        FrameData *m_recon;
        FrameData *m_lowres; // used by lookahead

        ModeInfo *m_modeInfo;

        LoopFilterMask *m_lfm;
        FrameData *m_lf;

        // cdef alg
        int8_t *m_cdefStrenths;
        int8_t (* m_cdefDirs)[8][8];
        int32_t (* m_cdefVars)[8][8];
        std::vector<int32_t> m_sbIndex;
        int32_t (*m_mse[2])[STRENGTH_COUNT_FAST];

        CdefLineBuffers m_cdefLineBufs;
        volatile uint32_t m_sbCount;

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
        int32_t m_miniGopCount;
        int32_t m_biFramesInMiniGop;
        int32_t m_frameOrderOfLastIdr;
        int32_t m_frameOrderOfLastIntra;
        int32_t m_frameOrderOfLastIntraInEncOrder;
        int32_t m_frameOrderOfLastAnchor;
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

        Frame *m_dpb[16];
        Frame *m_dpbVP9[NUM_REF_FRAMES]; // VP9 specific
        Frame *m_dpbVP9NextFrame[NUM_REF_FRAMES]; // VP9 specific
        Frame *m_prevFrame; // VP9 specific: used for PrevMvs & PrevRefFrames, shared ownership
        bool m_prevFrameIsOneOfRefs;
        int32_t m_dpbSize;

        uint8_t m_sliceQpY;
        std::vector<int8_t> m_lcuQps; // array for LCU QPs
        std::vector<uint8_t> m_lcuRound; // array for LCU Rounding Factors

        // for (frame) threading
        ThreadingTask *m_ttEncode;
        ThreadingTask *m_ttDeblockAndCdef;
        //ThreadingTask  m_ttCdefSync;
        //ThreadingTask  m_ttCdefApply;

        ThreadingTask  m_ttPackHeader;
        std::vector<ThreadingTask> m_ttPackTile;
        ThreadingTask  m_ttEncComplete;
        ThreadingTask  m_ttInitNewFrame;
        ThreadingTask  m_ttSubmitGpuCopySrc;
        ThreadingTask  m_ttSubmitGpuCopyRec;
        ThreadingTask  m_ttSubmitGpuIntra;
        ThreadingTask  m_ttSubmitGpuMe[AV1_ALTREF_FRAME+1];
        ThreadingTask  m_ttWaitGpuCopyRec;
        ThreadingTask  m_ttWaitGpuIntra;
        ThreadingTask  m_ttWaitGpuMe[AV1_ALTREF_FRAME+1];
#if PROTOTYPE_GPU_MODE_DECISION
        ThreadingTask  m_ttSubmitGpuMd;
        ThreadingTask  m_ttWaitGpuMd;
#else // PROTOTYPE_GPU_MODE_DECISION
        ThreadingTask  m_ttWaitGpuMePu;
#endif // PROTOTYPE_GPU_MODE_DECISION
        std::vector<ThreadingTask> m_ttLookahead;

        uint32_t m_numThreadingTasks;
        volatile uint32_t m_numFinishedThreadingTasks;
#if USE_CMODEL_PAK
        volatile uint32_t m_numFinishedTiles;
#endif

        // complexity statistics
        // 0 - original resolution, 1 - lowres
        Statistics* m_stats[2];
        SceneStats* m_sceneStats;

        // BRC info
        double m_avCmplx;
        double m_CmplxQstep;
        int32_t m_qpBase;
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
        FeiOutData   *m_feiIntraAngModes[4];
        FeiOutData   *m_feiInterData[AV1_ALTREF_FRAME+1][4];
        FeiSurfaceUp *m_feiModeInfo1;
        FeiSurfaceUp *m_feiModeInfo2;
        FeiBufferUp  *m_feiVarTxInfo;
        FeiSurfaceUp *m_feiRs8;
        FeiSurfaceUp *m_feiCs8;
        FeiSurfaceUp *m_feiRs16;
        FeiSurfaceUp *m_feiCs16;
        FeiSurfaceUp *m_feiRs32;
        FeiSurfaceUp *m_feiCs32;
        FeiSurfaceUp *m_feiRs64;
        FeiSurfaceUp *m_feiCs64;

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
        std::vector<TileContexts> m_tileContexts;
        int32_t segmentationEnabled;
        int32_t segmentationUpdateMap;
        int32_t showFrame;
        int32_t showExistingFrame; // 1 for frames which are encoded not in-order, 0 for normal frames
        int32_t frameToShowMapIdx; // frames which are encoded not in-order repeated later
        int32_t compFixedRef; // calculated based on refFrameSignBias
        int32_t compVarRef[2]; // calculated based on refFrameSignBias
        int32_t curFrameOffset;
        TplMvRef *m_tplMvs;

        CostType m_lambda;
        CostType m_lambdaSatd;
        uint64_t m_lambdaSatdInt; //  = int(m_lambdaSatd * (1<<24))
        BitCounts bitCount;

        LoopFilterFrameParam m_loopFilterParam;
        CdefParam m_cdefParam;

        uint16_t *m_txkTypes4x4;
        uint32_t isAV1;
        const AV1VideoParam *m_par;

        Frame()
        {
            ResetMemInfo();
            ResetEncInfo();
        }

        ~Frame() { Destroy(); }

        uint8_t wasLAProcessed()    { return m_wasLookAheadProcessed; }
        void setWasLAProcessed() { m_wasLookAheadProcessed = true; }
        void unsetWasLAProcessed() { m_wasLookAheadProcessed = false; }
        int32_t IsIntra() const { return ((m_picCodeType & MFX_FRAMETYPE_I) || intraOnly); }

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
    typedef std::list<Frame*>::iterator   FrameIter;

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
            for (typename std::vector<T*>::iterator i = m_objects.begin(); i != m_objects.end(); ++i)
                if (vm_interlocked_cas32(&(*i)->m_refCounter, 1, 0) == 0)
                    return *i;

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
                if (vm_interlocked_cas32(&(*i)->finished, 0, 1) == 1) {
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
