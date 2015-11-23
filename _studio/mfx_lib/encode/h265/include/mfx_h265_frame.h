//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_FRAME_H__
#define __MFX_H265_FRAME_H__

#include <memory>
#include <list>
#include <vector>
#include "vm_interlocked.h"
#include "umc_mutex.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_set.h"
#include "mfx_h265_fei.h"

namespace H265Enc {

    struct H265VideoParam;

    struct H265MV
    {
        Ipp16s  mvx;
        Ipp16s  mvy;
    };

    class State
    {
    public:
        State()
            : m_free(true)
        {
        }

        bool IsFree() const
        {
            return m_free;
        }

        void SetFree(bool free)
        {
            m_free = free;
        }

    private:
        bool m_free; // resource in use or could be reused (m_free == true means could be reused).
    };

    struct RefCounter
    {
        RefCounter()
            : m_refCounter(0)
        {}
        volatile Ipp32u m_refCounter; // to prevent race condition in case of frame threading
        void AddRef(void)  { vm_interlocked_inc32(&m_refCounter);};
        void Release(void) { vm_interlocked_dec32(&m_refCounter);}; // !!! here not to delete in case of refCounter == 0 !!!
    };

    struct Statistics : public RefCounter
    {
        std::vector<Ipp32s> m_interSad;
        std::vector<Ipp32s> m_interSad_pdist_past;
        std::vector<Ipp32s> m_interSad_pdist_future;

        std::vector<H265MV> m_mv;
        std::vector<H265MV> m_mv_pdist_future;
        std::vector<H265MV> m_mv_pdist_past;

        std::vector<Ipp32s> m_rs;
        std::vector<Ipp32s> m_cs;
        std::vector<Ipp64f> rscs_ctb;
        std::vector<Ipp32s> sc_mask;
        std::vector<Ipp32s> qp_mask;
        std::vector<Ipp32s> coloc_past;
        std::vector<Ipp32s> coloc_futr;

        std::vector<Ipp32s> m_intraSatd;
        std::vector<Ipp32s> m_interSatd;

        Ipp64f m_frameRs;
        Ipp64f m_frameCs;
        Ipp64f SC;
        Ipp64f TSC;
        Ipp64f avgsqrSCpp;
        Ipp64f avgTSC;
        Ipp32f m_avgBestSatd; //= sum( min{ interSatd[i], intraSatd[i] } ) / {winth*height};
        Ipp32f m_avgIntraSatd;
        Ipp32f m_avgInterSatd;
        Ipp32f m_intraRatio;
        Ipp32s m_sceneCut;
        Ipp64s m_metric;// special metric per frame for sceneCut

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
            m_metric = 0;
        }

        struct AllocInfo { Ipp32s width, height; };
        void Create(const AllocInfo &allocInfo);
        ~Statistics() { Destroy(); }
        void Destroy();
    };

    struct FrameData : public RefCounter, public NonCopyable
    {
        Ipp8u *y;
        Ipp8u *uv;
        mfxHDL m_handle;
        Ipp32s width;
        Ipp32s height;
        Ipp32s padding;
        Ipp32s pitch_luma_pix;
        Ipp32s pitch_luma_bytes;
        Ipp32s pitch_chroma_pix;
        Ipp32s pitch_chroma_bytes;

        FrameData() : y(), uv(), m_handle(), mem(), m_fei() {}
        ~FrameData() { Destroy(); }

        struct AllocInfo {
            Ipp32s width;       // in elements
            Ipp32s height;      // in elements
            Ipp32s paddingLu;   // in elements
            Ipp32s paddingChW;  // in elements
            Ipp32s paddingChH;  // in elements
            Ipp32s bitDepthLu;
            Ipp32s bitDepthCh;
            Ipp32s chromaFormat;
            // for GACC should be requested from CmDevice
            void  *feiHdl;
            Ipp32s alignment;
            Ipp32s pitchInBytesLu;
            Ipp32s pitchInBytesCh;
            Ipp32s sizeInBytesLu;
            Ipp32s sizeInBytesCh;
        };

        void Create(const AllocInfo &allocInfo);
        void Destroy();

    private:
        Ipp8u *mem;
        void  *m_fei;
    };

    struct FeiInputData : public RefCounter
    {
        FeiInputData() : m_fei(NULL), m_handle(NULL) {}
        ~FeiInputData() { Destroy(); }

        struct AllocInfo { void *feiHdl; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void *m_fei;
        mfxHDL m_handle;
    };

    struct FeiReconData : public RefCounter
    {
        FeiReconData() : m_fei(NULL), m_handle(NULL) {}
        ~FeiReconData() { Destroy(); }

        struct AllocInfo { void *feiHdl; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void *m_fei;
        mfxHDL m_handle;
    };

    struct FeiBufferUp : public RefCounter
    {
        FeiBufferUp() : m_fei(), m_handle(), m_allocated(), m_sysmem() {}
        ~FeiBufferUp() { Destroy(); }
        struct AllocInfo { mfxHDL feiHdl; Ipp32u size, alignment; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void  *m_fei;
        mfxHDL m_handle;
        Ipp8u *m_allocated;
        Ipp8u *m_sysmem;
    };

    struct FeiOutData : public RefCounter
    {
        FeiOutData() : m_fei(NULL), m_handle(NULL), m_sysmem(NULL), m_pitch(0) {}
        ~FeiOutData() { Destroy(); }
        struct AllocInfo { void *feiHdl; mfxSurfInfoENC allocInfo; };
        void Create(const AllocInfo &allocInfo);
        void Destroy();

        void *m_fei;
        mfxHDL m_handle;
        mfxU8 *m_sysmem;
        Ipp32s m_pitch;
    };


    struct SaoOffsetOut_gfx
    {
        Ipp8u mode_idx;
        Ipp8u type_idx;
        Ipp8u startBand_idx;
        Ipp8u saoMaxOffsetQVal;
        Ipp16s offset[4];
        Ipp8u reserved[4];
    }; // sizeof() == 16 bytes


    struct H265CUData;
    class Frame : public RefCounter
    {
    public:
        FrameData *m_origin;
        FrameData *m_recon;
        FrameData *m_lowres; // used by lookahead

        H265CUData *cu_data;

        Ipp8u  m_bitDepthLuma;
        Ipp8u  m_bdLumaFlag;
        Ipp8u  m_bitDepthChroma;
        Ipp8u  m_bdChromaFlag;
        Ipp8u  m_chromaFormatIdc;

        Ipp32s m_picStruct;
        Ipp32s m_secondFieldFlag;
        Ipp32s m_bottomFieldFlag;
        mfxU64 m_timeStamp;
        Ipp32u m_picCodeType;
        Ipp32s m_RPSIndex;

        // flags indicate that the stages were done on the frame
        Ipp8u  m_wasLookAheadProcessed;
        Ipp32u m_lookaheadRefCounter;

        Ipp32s m_pyramidLayer;
        Ipp32s m_miniGopCount;
        Ipp32s m_biFramesInMiniGop;
        Ipp32s m_frameOrder;
        Ipp32s m_frameOrderOfLastIdr;
        Ipp32s m_frameOrderOfLastIntra;
        Ipp32s m_frameOrderOfLastIntraInEncOrder;
        Ipp32s m_frameOrderOfLastAnchor;
        Ipp32s m_poc;
        Ipp32s m_encOrder;
        Ipp8u  m_isShortTermRef;
        Ipp8u  m_isLongTermRef;
        Ipp8u  m_isIdrPic;
        Ipp8u  m_isRef;
        Ipp8u  m_doPostProc;
        RefPicList m_refPicList[2]; // 2 reflists containing reference frames used by current frame
        Ipp32s m_mapRefIdxL1ToL0[MAX_NUM_REF_IDX];
        Ipp32s m_mapListRefUnique[2][MAX_NUM_REF_IDX];
        Ipp32s m_numRefUnique;
        Ipp32s m_allRefFramesAreFromThePast;
        Ipp32u m_frameType;// full info for bs. m_frameType = m_codeType | isIDR ? | isRef ?

        std::vector<H265Slice> m_slices;
        Frame *m_dpb[16];
        Ipp32s m_dpbSize;
        Ipp32s m_numPocTotalCurr; // NumPocTotalCurr from spec
        Ipp32s m_ceilLog2NumPocTotalCurr; // Ceil(Log2(NumPocTotalCurr))

        Ipp8s     m_sliceQpY;
        std::vector<Ipp8s> m_lcuQps; // array for LCU QPs
        H265Slice m_dqpSlice[2*MAX_DQP+1];

        std::vector<H265Slice> m_roiSlice;

        H265ShortTermRefPicSet m_shortRefPicSet[66];

        // for (frame) threading
        volatile Ipp32s m_codedRow; // sync info in case of frame threading
        ThreadingTask *m_threadingTasks;
        ThreadingTask  m_ttEncComplete;
        ThreadingTask  m_ttInitNewFrame;
        ThreadingTask  m_ttPadRecon;
        ThreadingTask  m_ttSubmitGpuCopySrc;
        ThreadingTask  m_ttSubmitGpuCopyRec;
        ThreadingTask  m_ttSubmitGpuIntra;
        ThreadingTask  m_ttSubmitGpuMe[4];
        ThreadingTask  m_ttSubmitGpuPostProc;
        ThreadingTask  m_ttWaitGpuCopyRec;
        ThreadingTask  m_ttWaitGpuIntra;
        ThreadingTask  m_ttWaitGpuMe[4];
        ThreadingTask  m_ttWaitGpuPostProc;

        Ipp32u m_numThreadingTasks;
        volatile Ipp32u m_numFinishedThreadingTasks;
        Ipp32s m_encIdx; // we have "N" frameEncoders. this index indicates owner of the frame [0, ..., N-1]

        // complexity/content statistics
        // 0 - original resolution, 1 - lowres
        Statistics* m_stats[2];

        // BRC info
        Ipp64f m_avCmplx;
        Ipp64f m_CmplxQstep;
        Ipp32s m_qpBase;
        Ipp64f m_totAvComplx;
        Ipp32s m_totComplxCnt;
        Ipp64f m_complxSum;
        Ipp32s m_predBits;
        Ipp64f m_cmplx;
        std::vector<Frame *> m_futureFrames;

        // FEI resources
        FeiInputData *m_feiOrigin;
        FeiReconData *m_feiRecon;
        FeiOutData   *m_feiIntraAngModes[4];
        FeiOutData   *m_feiInterData[4][4];
        FeiBufferUp  *m_feiCuData;
        FeiBufferUp  *m_feiSaoModes;
        void *m_feiSyncPoint;

        std::vector<mfxPayload> m_userSeiMessages;
        std::vector<mfxU8>      m_userSeiMessagesData;

        Frame()
        {
            ResetMemInfo();
            ResetEncInfo();
            ResetCounters();
        }

        ~Frame() { Destroy(); mem = NULL;}

        Ipp8u wasLAProcessed()    { return m_wasLookAheadProcessed; }
        void setWasLAProcessed() { m_wasLookAheadProcessed = true; }
        void unsetWasLAProcessed() { m_wasLookAheadProcessed = false; }

        void Create(H265VideoParam *par);
        void Destroy();
        void CopyFrameData(const mfxFrameSurface1 *in);

        void ResetMemInfo();
        void ResetEncInfo();
        void ResetCounters();

        private:
            void *mem;
    };

    typedef std::list<Frame*> FrameList;
    typedef std::list<Frame*>::iterator   FrameIter;

    void Dump(H265VideoParam *par, Frame* frame, FrameList & dpb);

    template <class T> void PadRect(T *plane, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth, Ipp32s padw, Ipp32s padh);
    void PadRectLuma         (const FrameData &fdata, Ipp32s fourcc, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth);
    void PadRectChroma       (const FrameData &fdata, Ipp32s fourcc, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth);
    void PadRectLumaAndChroma(const FrameData &fdata, Ipp32s fourcc, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth);

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

        void Init(const typename T::AllocInfo &allocInfo, Ipp32s numPrealloc)
        {
            assert(m_objects.size() == 0);
            m_allocInfo = allocInfo;

            for (Ipp32s i = 0; i < numPrealloc; i++)
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

            std::auto_ptr<T> newFrame(new T());
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
            std::auto_ptr<ThreadingTask> newHub(new ThreadingTask());
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

#endif // __MFX_H265_FRAME_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
