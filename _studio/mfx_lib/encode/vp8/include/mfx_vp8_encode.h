//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VP8_VIDEO_ENCODE)

#ifndef __MFX_VP8_ENCODE_H__
#define __MFX_VP8_ENCODE_H__

#include "mfxvp8.h"
#include "vm_thread.h"
#include "vm_event.h"

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfx_ext_buffers.h"

class MFXVideoENCODEVP8;

struct VP8EncodeTaskInputParams
{
    mfxEncodeCtrl *ctrl;
    mfxFrameSurface1 *surface;
    mfxBitstream *bs;
};

extern "C" typedef int (*threads_fun_type)(void *);

class MFXVideoENCODEVP8 : public VideoENCODE {
public:
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void)
    {
        return MFX_TASK_THREADING_INTRA;
    }

    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoENCODEVP8(VideoCORE *core, mfxStatus *status);
    virtual ~MFXVideoENCODEVP8();
    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);

    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams,  MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs);
    virtual mfxStatus CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/) {return MFX_ERR_UNSUPPORTED;}    

    void ParallelRegionStart(threads_fun_type threads_fun, int num_threads, void *threads_data, int threads_data_size);
    void ParallelRegionEnd();

private:

    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *surface);

    mfxStatus WriteEndOfSequence(mfxBitstream *bs);
    mfxStatus WriteEndOfStream(mfxBitstream *bs);
    mfxStatus WriteFillerData(mfxBitstream *bs, mfxI32 num);

    //virtual mfxStatus InsertUserData(mfxU8 *ud, mfxU32 len, mfxU64 ts);
    mfxI32 SelectFrameType_forLast(mfxI32 frameNum, bool bNotLast, mfxEncodeCtrl *ctrl);
    mfxStatus InitVpxCfg(mfxHDL _cfg, mfxVideoParam *par, mfxExtCodingOptionVP8 *opts, int (*arg_ctrls)[2], int *arg_ctrl_cnt);


    struct EncodeFrameTaskParams
    {
        // Counter to determine the thread which does single threading tasks
        volatile mfxI32 single_thread_selected;
        // Variable which distributes thread number among parallel tasks after single thread code finishes
        volatile mfxI32 thread_number;
        // Variable which specifies parallel region
//        volatile mfxI32 parallel_region_selector;
        // Flag which signals that code before parallel region is finished
        volatile bool parallel_execution_in_progress;
        // Event which allows single thread to wait for all parallel executing threads
        vm_event parallel_region_done;
        // Counter for threads that work in addition to the single thread
        volatile mfxI32 parallel_executing_threads;
        // Signal that parallel tasks may exit pRoutine
        volatile bool parallel_encoding_finished;
        // Flag that parallel cabac encoding is enabled for MBT
        volatile mfxI32 cabac_encoding_in_progress;
        // Number of threads excluding main thread
        volatile mfxI32 num_threads;

        threads_fun_type threads_fun;

        void *threads_data;
        int threads_data_size;
    } m_taskParams;

    static mfxStatus TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes);

    VideoCORE *m_core;

    mfxVideoParam              m_mfxVideoParam;
    mfxExtCodingOptionVP8      m_extOption;
    mfxExtCodingOptionVP8Param m_extVP8Par;
    mfxExtOpaqueSurfaceAlloc   m_extOpaqAlloc;

    mfxU32  m_frameCountSync;         // counter for sync. part
    mfxU32  m_frameCount;             // counter for Async. part

    mfxU32  m_totalBits;
    mfxU32  m_encodedFrames;
    bool    m_isInitialized;
    bool    m_putClockTimestamp; //write clock timestamps
    
    bool    m_useAuxInput;

    mfxFrameSurface1        m_auxInput;
    mfxFrameAllocResponse   m_response;
    mfxFrameAllocResponse   m_response_alien;
    mfxU16                  m_initFrameW;
    mfxU16                  m_initFrameH;


    // VPX
    int    vpx_arg_deadline;
    mfxHDL vpx_codec; // vpx_codec_ctx_t
    mfxHDL vpx_cfg;   // vpx_codec_enc_cfg_t
};


#endif // __MFX_VP8_ENCODE_H__
#endif // MFX_ENABLE_VP8_VIDEO_ENCODE
/* EOF */
