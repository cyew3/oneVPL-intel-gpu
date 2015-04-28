//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2015 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "memory"
#include "deque"

#include "ippdefs.h"

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfx_ext_buffers.h"

#include "mfx_h265_set.h"
#include "mfx_h265_enc.h"

namespace H265Enc {

    class H265FrameEncoder;
    class H265BRC;
    class Lookahead;

#if defined(MFX_VA)
    class FeiContext;
#endif

    class Hrd
    {
    public:
        Hrd() { Zero(*this); }
        void Init(const H265SeqParameterSet &sps, Ipp32u initialDelay);
        void Update(Ipp32u sizeInbits, const Frame &pic);
        Ipp32u GetInitCpbRemovalDelay() const;
        Ipp32u GetInitCpbRemovalOffset() const;
        Ipp32u GetAuCpbRemovalDelayMinus1(const Frame &pic) const;
        
    protected:
        Ipp8u  cbrFlag;
        Ipp32u bitrate;
        Ipp32u maxCpbRemovalDelay;
        double clockTick;
        double cpbSize90k;
        double initCpbRemovalDelay;

        Ipp32u prevAuCpbRemovalDelayMinus1;
        Ipp32u prevAuCpbRemovalDelayMsb;
        double prevAuFinalArrivalTime;
        double prevBuffPeriodAuNominalRemovalTime;
        Ipp32u prevBuffPeriodEncOrder;
    };

    class H265Encoder : public NonCopyable {
    public:
        explicit H265Encoder(VideoCORE &core);
        ~H265Encoder();

        mfxStatus Init(const mfxVideoParam &par);
        mfxStatus Reset(const mfxVideoParam &par);

        const Ipp8u *GetSps(Ipp16u &size) const { size = m_spsBufSize; return m_spsBuf; }
        const Ipp8u *GetPps(Ipp16u &size) const { size = m_ppsBufSize; return m_ppsBuf; }

        void GetEncodeStat(mfxEncodeStat &stat);

        mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, MFX_ENTRY_POINT &entryPoint);

        void Close();
       
    protected:
        mfxStatus AllocAuxFrame();

        // ------ mfx level
        VideoCORE &m_core;
        H265Enc::H265VideoParam m_videoParam;
        H265Enc::H265VideoParam m_videoParam_lowres;
        H265Enc::H265VideoParam m_videoParam_8bit;
        Hrd m_hrd;

        mfxEncodeStat m_stat;
        MfxMutex m_statMutex;

        // frame encoder
        std::vector<H265FrameEncoder*> m_frameEncoder;

        mfxU32  m_frameCountSync; // counter for sync. part
        mfxU32  m_frameCountBufferedSync;
        mfxU32  m_taskID;

        bool    m_useSysOpaq;
        bool    m_useVideoOpaq;
        bool    m_isOpaque;

        bool    m_useAuxInput;
        mfxFrameSurface1 m_auxInput;
        mfxFrameAllocResponse m_responseAux;
    
        H265ProfileLevelSet m_profile_level;
        H265VidParameterSet m_vps;
        H265SeqParameterSet m_sps;
        H265PicParameterSet m_pps;
        ActiveParameterSets m_seiAps;
        Ipp8u m_spsBuf[1024];
        Ipp8u m_ppsBuf[1024];
        Ipp16u m_spsBufSize;
        Ipp16u m_ppsBufSize;

        // input frame control counters
        Ipp32s m_profileIndex;
        Ipp32s m_frameOrder;
        Ipp32s m_frameOrderOfLastIdr;               // frame order of last IDR frame (in display order)
        Ipp32s m_frameOrderOfLastIntra;             // frame order of last I-frame (in display order)
        Ipp32s m_frameOrderOfLastIntraInEncOrder;   // frame order of last I-frame (in encoding order)
        Ipp32s m_frameOrderOfLastAnchor;            // frame order of last anchor (first in minigop) frame (in display order)
        Ipp32s m_frameOrderOfLastIdrB;              // (is used for encoded order) frame order of last IDR frame (in display order)
        Ipp32s m_frameOrderOfLastIntraB;            // (is used for encoded order) frame order of last I-frame (in display order)
        Ipp32s m_frameOrderOfLastAnchorB;           // (is used for encoded order)frame order of last anchor (first in minigop) frame (in display order)
        Ipp32s m_LastbiFramesInMiniGop;             // (is used for encoded order)
        Ipp32s m_miniGopCount;
        mfxU64 m_lastTimeStamp;
        Ipp32s m_lastEncOrder;

        // threading
        vm_cond m_condVar;
        vm_mutex m_critSect;
        std::deque<ThreadingTask *> m_pendingTasks;
        ThreadingTask   m_threadingTaskComplete;
        ThreadingTask   m_threadingTaskInitNewFrame;
        ThreadingTask   m_threadingTaskGpuCurr;
        ThreadingTask   m_threadingTaskGpuNext;
        volatile Ipp32u m_threadingTaskRunning;
        std::vector<Ipp32u> m_ithreadPool;
    
        // frame flow-control queues
        std::list<Frame *> m_free;            // _global_ free frame pool
        std::list<Frame *> m_inputQueue;      // _global_ input frame queue in _display_ order
        std::list<Frame *> m_lookaheadQueue;  // _global_ input frame queue in _display_ order
        std::list<Frame *> m_reorderedQueue;  // _global_ input frame queue in _encode_ order (ref list is _invalid_)
        std::list<Frame *> m_encodeQueue;     // _global_ queue in _encode_ order (ref list _valid_)
        std::list<Frame *> m_outputQueue;     // _global_ queue in _encode_ order ( submitted to encoding)

        std::list<Frame *> m_dpb;             // _global_ pool of frames: encoded reference frames + being encoded frames
        std::list<Frame *> m_actualDpb;       // reference frames for next frame to encode

        // resources
        std::vector<FrameData *> m_originPool;      // _global_ free (origin/recon/reference) pool
        std::vector<FrameData *> m_originPool_8bit; // origin/recon in 8bit. used by fei in enc:p010 mode 
        std::vector<FrameData *> m_lowresPool;

        std::vector<Statistics *> m_statsPool;      // pool of original statistics / per frame
        std::vector<Statistics *> m_lowresStatsPool;// pool of lowres statistics / per frame
    
        Ipp8u* m_memBuf;
        void *m_cu;
        Ipp16u *m_tile_ids;
        Segment *m_tiles;
        Ipp16u *m_slice_ids;
        Segment *m_slices;
        // perCU (num_threads_structs)
        H265BsFake* m_bsf;
        H265CUData *data_temp;

        BrcIface *m_brc;
        std::auto_ptr<Lookahead> m_la;

#if defined(MFX_VA)
        FeiContext *m_FeiCtx;
#endif

        mfxStatus Init_Internal();

        // ------SPS, PPS
        void SetProfileLevel();
        void SetVPS();
        void SetSPS();
        void SetPPS();
        void SetSeiAps();
        void SetSlice(H265Slice *slice, Ipp32u curr_slice, Frame* currentFrame);

        // ------ _global_ stages of Input Frame Control
        mfxStatus AcceptFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl *ctrl, mfxBitstream *mfxBS);

        // threading tasks
        void InitNewFrame(Frame *out, mfxFrameSurface1 *in);
        void ProcessFrameOnGpu(const Frame *frame, Ipp32s isAhead);

        friend class H265Enc::Lookahead;
        friend class H265Enc::H265FrameEncoder;
        void ConfigureInputFrame(Frame *frame, bool bEncOrder) const;
        void UpdateGopCounters(Frame *frame, bool bEncOrder);
        void RestoreGopCountersFromFrame(Frame *frame, bool bEncOrder);

        void ConfigureEncodeFrame(Frame *frame);
        mfxStatus EnqueueFrameEncoder(int& encIdx);// find next frame and free encoder, bind them and return encIdx [-1(not found resources), or 0, ..., N-1]
        void OnEncodingQueried(Frame *encoded);
    
        // ------ Ref List Managment
        void CreateRefPicList(Frame *frame, H265ShortTermRefPicSet *rps);
        void InitShortTermRefPicSet();
        void UpdateDpb(Frame *currTask);
        void CleanGlobalDpb();

        // ------ threading
        static mfxStatus TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
        static mfxStatus TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes);

        void PrepareToEncode(void *pParam); // no threas safety. some preparation work (accept new input frame, configuration, fei, paq etc) in single thread mode!
        mfxStatus SyncOnFrameCompletion(Frame* frame, mfxBitstream* mfxBs, void *pParam);

        void RunLookahead();
        void OnLookaheadStarting(); // no threas safety. some preparation work in single thread mode!
        void OnLookaheadCompletion(); // no threas safety. some post work in single thread mode!

        // -------FEI
        Ipp8u m_have8bitCopyFlag;
    };
};


#endif // MFX_ENABLE_H265_VIDEO_ENCODE
