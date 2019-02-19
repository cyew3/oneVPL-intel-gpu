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

#include "memory"
#include "deque"
#include "ippdefs.h"
#include "umc_mutex.h"
#include "mfx_av1_defs.h"
#include "mfx_av1_enc.h"

#include <condition_variable>
#include <thread>

class CmDevice;
class MFXCoreInterface1;
struct AV1ShortTermRefPicSet;

namespace AV1Enc {

    class AV1FrameEncoder;
    class AV1BRC;
    class Lookahead;
    struct AV1EncodeTaskInputParams;

    class AV1Encoder : public NonCopyable {
    public:
        explicit AV1Encoder(MFXCoreInterface1 &core);
        ~AV1Encoder();

        mfxStatus Init(const mfxVideoParam &par);
        mfxStatus Reset(const mfxVideoParam &par);

        const uint8_t *GetSps(uint16_t &size) const { size = m_spsBufSize; return m_spsBuf; }

        void GetEncodeStat(mfxEncodeStat &stat);

        mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, MFX_ENTRY_POINT &entryPoint);

        void Close();

    protected:
        // ------ mfx level
        MFXCoreInterface1 &m_core;
        AV1VideoParam m_videoParam;

        mfxEncodeStat m_stat;
        MfxMutex m_statMutex;

        // frame encoder
        std::vector<AV1FrameEncoder*> m_frameEncoder;

        mfxU32  m_frameCountSync; // counter for sync. part
        mfxU32  m_frameCountBufferedSync;

        bool    m_useSysOpaq;
        bool    m_useVideoOpaq;
        bool    m_isOpaque;

        uint8_t m_spsBuf[1024];
        int32_t m_spsBufSize;

        // input frame control counters
        int32_t m_profileIndex;
        int32_t m_frameOrder;
        int32_t m_frameOrderOfLastIdr;               // frame order of last IDR frame (in display order)
        int32_t m_frameOrderOfLastIntra;             // frame order of last I-frame (in display order)
        int32_t m_frameOrderOfLastIntraInEncOrder;   // frame order of last I-frame (in encoding order)
        int32_t m_frameOrderOfLastAnchor;            // frame order of last anchor (first in minigop) frame (in display order)
        int32_t m_frameOrderOfLastIdrB;              // (is used for encoded order) frame order of last IDR frame (in display order)
        int32_t m_frameOrderOfLastIntraB;            // (is used for encoded order) frame order of last I-frame (in display order)
        int32_t m_frameOrderOfLastAnchorB;           // (is used for encoded order)frame order of last anchor (first in minigop) frame (in display order)
        int32_t m_LastbiFramesInMiniGop;             // (is used for encoded order)
        int32_t m_miniGopCount;
        uint64_t m_lastTimeStamp;
        int32_t m_lastEncOrder;
        int32_t m_sceneOrder;                        // in display order each frame belongs something scene (if sceneCut enabled)

        uint8_t m_last_filter_level;

        int32_t m_refFrameId[NUM_REF_FRAMES];

        // threading
        volatile uint32_t   m_doStage;
        volatile uint32_t   m_threadCount;
        volatile uint32_t   m_reencode;     // BRC repack
        volatile uint32_t   m_taskSubmitCount;
        volatile uint32_t   m_taskEncodeCount;

        int32_t m_nextFrameToEncode; // frame counter in display order
        Frame *m_nextFrameDpbVP9[NUM_REF_FRAMES]; // pocs of references frames available for nextFrameToEncode

        std::condition_variable m_condVar;
        std::mutex m_critSect;
        std::mutex m_prepCritSect;
        std::deque<ThreadingTask *> m_pendingTasks;
        std::deque<AV1EncodeTaskInputParams *> m_inputTasks;
        AV1EncodeTaskInputParams *m_inputTaskInProgress;
        volatile uint32_t m_threadingTaskRunning;
        std::vector<uint32_t> m_ithreadPool;

        // frame flow-control queues
        std::list<Frame *> m_free;            // _global_ free frame pool
        std::list<Frame *> m_inputQueue;      // _global_ input frame queue in _display_ order
        std::list<Frame *> m_lookaheadQueue;  // _global_ input frame queue in _display_ order
        std::list<Frame *> m_reorderedQueue;  // _global_ input frame queue in _encode_ order (ref list is _invalid_)
        std::list<Frame *> m_encodeQueue;     // _global_ queue in _encode_ order (ref list _valid_)
        std::list<Frame *> m_outputQueue;     // _global_ queue in _encode_ order ( submitted to encoding)

        std::list<Frame *> m_dpb;             // _global_ pool of frames: encoded reference frames + being encoded frames
        std::list<Frame *> m_actualDpb;       // reference frames for next frame to encode
        Frame *m_laFrame[2];

        int32_t m_lastShowFrame;
        Frame *m_prevFrame; // VP9 specific: used for PrevMvs & PrevRefFrames, shared ownership
        Frame *m_actualDpbVP9[NUM_REF_FRAMES]; // VP9 specific: VP9-style DPB
        FrameContexts m_contexts[8];
        EntropyContexts m_cdfs[8];

        ObjectPool<FrameData>     m_inputFrameDataPool;     // storage for full-sized original pixel data
        ObjectPool<FrameData>     m_input10FrameDataPool;     // storage for full-sized original pixel data for 10 bit
        ObjectPool<FrameData>     m_reconFrameDataPool;     // storage for full-sized reconstructed/reference pixel data
        ObjectPool<FrameData>     m_lfFrameDataPool;        // storage for full-sized loop filter pixel data
        ObjectPool<FrameData>     m_frameDataLowresPool;    // storage for lowres original pixel data for lookahead
        ObjectPool<Statistics>    m_statsPool;              // storage for full-sized statistics per frame
        ObjectPool<Statistics>    m_statsLowresPool;        // storage for lowres statistics per frame
        ObjectPool<SceneStats>    m_sceneStatsPool;         // storage for scenecut statistics per frame
        ObjectPool<FeiInputData>  m_feiInputDataPool;       // storage for full-sized original pixel data
        ObjectPool<FeiReconData>  m_feiReconDataPool;       // storage for full-sized reconstructed/reference pixel data
        ObjectPool<FeiOutData>    m_feiInterDataPool[4];    // storage for ME motion vectors and distortions output by fei (8x8, 16x16, 32x32, 64x64)
        ObjectPool<FeiSurfaceUp>  m_feiModeInfoPool;        // storage for mode info shared between CPU and GPU (2 buffer/frame)
        ObjectPool<FeiBufferUp>   m_feiVarTxInfoPool;       // storage for mode info shared between CPU and GPU (2 buffer/frame)
        ObjectPool<FeiSurfaceUp>  m_feiRsCs8Pool;           // storage for rscs info shared between CPU and GPU (2 buffer/frame)
        ObjectPool<FeiSurfaceUp>  m_feiRsCs16Pool;          // storage for rscs info shared between CPU and GPU (2 buffer/frame)
        ObjectPool<FeiSurfaceUp>  m_feiRsCs32Pool;          // storage for rscs info shared between CPU and GPU (2 buffer/frame)
        ObjectPool<FeiSurfaceUp>  m_feiRsCs64Pool;          // storage for rscs info shared between CPU and GPU (2 buffer/frame)
        ObjectPool<ThreadingTask> m_ttHubPool;              // storage for threading tasks of type TT_HUB

        uint8_t* m_memBuf;
        void *m_cu;
        // perCU (num_threads_structs)
        ModeInfo *data_temp;

        BrcIface *m_brc;
        std::unique_ptr<Lookahead> m_la;


        static uint32_t VM_CALLCONVENTION FeiThreadRoutineStarter(void *p);
        void FeiThreadRoutine();
        void FeiThreadSubmit(ThreadingTask &task);
        bool FeiThreadWait(uint32_t timeout);
        void *m_fei;
        std::thread m_feiThread;
        int32_t m_pauseFeiThread;
        int32_t m_stopFeiThread;
        volatile int32_t m_feiThreadRunning;
        std::deque<ThreadingTask *> m_feiSubmitTasks;
        std::deque<ThreadingTask *> m_feiWaitTasks;
        std::condition_variable m_feiCondVar;
        std::mutex m_feiCritSect;

        mfxStatus InitInternal();

        // ------ _global_ stages of Input Frame Control
        Frame *AcceptFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl *ctrl, mfxBitstream *mfxBS, int32_t fieldNum);

        // new (incoming) frame
        void InitNewFrame(Frame *out, mfxFrameSurface1 *in);
        void PerformPadRecon(Frame *frame);

        friend class AV1Enc::Lookahead;
        friend class AV1Enc::AV1FrameEncoder;
        void ConfigureInputFrame(Frame *frame, bool bEncOrder, bool bEncCtrl = false) const;
        void UpdateGopCounters(Frame *frame, bool bEncOrder);
        void RestoreGopCountersFromFrame(Frame *frame, bool bEncOrder);

        void ConfigureEncodeFrame(Frame *frame);
        void OnEncodingQueried(Frame *encoded);

        // ------ Ref List Managment
        void CreateRefPicList(Frame *frame, AV1ShortTermRefPicSet *rps);
        void UpdateDpb(Frame *currTask);
        void CleanGlobalDpb();

        // ------ threading
        static mfxStatus TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
        static mfxStatus TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes);

        void PrepareToEncodeNewTask(AV1EncodeTaskInputParams *inputParam); // accept frame and find frame for lookahead
        void PrepareToEncode(AV1EncodeTaskInputParams *inputParam); // build dependency graph for all parallel frames
        void EnqueueFrameEncoder(AV1EncodeTaskInputParams *inputParam); // build dependency graph for one frames
        mfxStatus SyncOnFrameCompletion(AV1EncodeTaskInputParams *inputParam, Frame *frame);

        void OnLookaheadStarting(); // no threas safety. some preparation work in single thread mode!
        void OnLookaheadCompletion(); // no threas safety. some post work in single thread mode!
    };

    template <class PixType> void CopyAndPad(const mfxFrameSurface1 &src, FrameData &dst, uint32_t fourcc);
};


#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
