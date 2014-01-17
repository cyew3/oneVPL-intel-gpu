//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_ENCODE_H__
#define __MFX_H265_ENCODE_H__

#include "ippdefs.h"

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfx_ext_buffers.h"

#include "mfx_umc_alloc_wrapper.h"

namespace H265Enc {
    class H265Encoder;
}

class MFXVideoENCODEH265 : public VideoENCODE {
public:
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void)
    {
        return MFX_TASK_THREADING_INTRA;
    }

    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoENCODEH265(VideoCORE *core, mfxStatus *status);
    virtual ~MFXVideoENCODEH265();
    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);

    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams);
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs);
    virtual mfxStatus CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/) {return MFX_ERR_UNSUPPORTED;}

    //  threading
    void ParallelRegionStart(int num_threads, Ipp32s region_selector);
    void ParallelRegionEnd();

    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *surface);

protected:
// ------ mfx level
    VideoCORE *m_core;
    mfxVideoParam          m_mfxVideoParam;
    mfxExtCodingOptionHEVC m_mfxHEVCOpts;
    mfxExtDumpFiles        m_mfxDumpFiles;
    H265Enc::H265Encoder  *m_enc;

    mfxU32                 m_frameCountSync; // counter for sync. part
    mfxU32                 m_frameCount;     // counter for Async. part
    mfxU32                 m_frameCountBufferedSync;
    mfxU32                 m_frameCountBuffered;

    mfxU32  m_totalBits;
    mfxU32  m_encodedFrames;
    bool    m_isInitialized;

    bool    m_useSysOpaq;
    bool    m_useVideoOpaq;
    bool    m_isOpaque;

    bool    m_useAuxInput;
    mfxFrameSurface1 m_auxInput;
    mfxFrameAllocResponse m_response;
    mfxFrameAllocResponse m_response_alien;

//  threading
    static mfxStatus TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes);



    struct EncodeFrameTaskParams
    {
        // Counter to determine the thread which does single threading tasks
        volatile mfxI32 single_thread_selected;
        // Variable which distributes thread number among parallel tasks after single thread code finishes
        volatile mfxI32 thread_number;
        // Variable which specifies parallel region
        volatile mfxI32 parallel_region_selector;
        // Flag which signals that code before parallel region is finished
        volatile bool parallel_execution_in_progress;
        // Event which allows single thread to wait for all parallel executing threads
        vm_event parallel_region_done;
        // Counter for threads that work in addition to the single thread
        volatile mfxI32 parallel_executing_threads;
        // Signal that parallel tasks may exit pRoutine
        volatile bool parallel_encoding_finished;
        // Number of threads excluding main thread
        volatile mfxI32 num_threads;
        vm_mutex parallel_region_end_lock;
    } m_taskParams;
};

#endif // __MFX_H265_ENCODE_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
