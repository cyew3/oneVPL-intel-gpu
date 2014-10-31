//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_ENCODE_H__
#define __MFX_H265_ENCODE_H__

#include "ippdefs.h"
#include "umc_semaphore.h"

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfx_ext_buffers.h"

#include "mfx_h265_set.h"
#include "mfx_h265_enc.h"

using namespace H265Enc;
namespace H265Enc {
    class H265FrameEncoder;
    class H265BRC;
    struct Task;

#if defined(MFX_VA)
    class FeiContext;
#endif
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
    virtual mfxStatus Close(void);

    virtual mfxStatus Reset(mfxVideoParam *par);
    

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);

    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams);

    virtual mfxStatus EncodeFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/) {return MFX_ERR_UNSUPPORTED;};
    virtual mfxStatus AcceptFrameHelper(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs);

    virtual mfxStatus CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/) {return MFX_ERR_UNSUPPORTED;}

    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *surface);


private:
// ------ mfx level
    VideoCORE *m_core;
    mfxVideoParam  m_mfxParam;
    H265Enc::H265VideoParam m_videoParam;

    mfxExtCodingOptionHEVC  m_mfxHEVCOpts;
    mfxExtDumpFiles         m_mfxDumpFiles;

    // frame encoder
    std::vector<H265FrameEncoder*> m_frameEncoder;

    mfxU32  m_frameCountSync; // counter for sync. part
    mfxU32  m_frameCount;     // counter for Async. part
    mfxU32  m_frameCountBufferedSync;
    mfxU32  m_frameCountBuffered;
    mfxU32  m_taskID;

    mfxU32  m_totalBits;
    mfxU32  m_encodedFrames;
    bool    m_isInitialized;

    bool    m_useSysOpaq;
    bool    m_useVideoOpaq;
    bool    m_isOpaque;

    bool    m_useAuxInput;
    mfxFrameSurface1 m_auxInput;
    mfxFrameAllocResponse m_response;
    mfxFrameAllocResponse m_responseAlien;
    const vm_char *m_recon_dump_file_name;
    
    H265ProfileLevelSet m_profile_level;
    H265VidParameterSet m_vps;
    H265SeqParameterSet m_sps;
    H265PicParameterSet m_pps;

    // input frame control counters
    Ipp32s m_profileIndex;
    Ipp32s m_frameOrder;
    Ipp32s m_frameOrderOfLastIdr;       // frame order of last IDR frame (in display order)
    Ipp32s m_frameOrderOfLastIntra;     // frame order of last I-frame (in display order)
    Ipp32s m_frameOrderOfLastAnchor;    // frame order of last anchor (first in minigop) frame (in display order)
    Ipp32s m_miniGopCount;
    mfxU64 m_lastTimeStamp;
    Ipp32s m_lastEncOrder;
    UMC::Semaphore m_semaphore;


    //  frame flow-control queues
    std::list<Task*> m_free;            // _global_ free task pool
    std::list<Task*> m_inputQueue;      // _global_ input task queue in _display_ order
    std::list<Task*> m_reorderedQueue;  // _global_ input task queue in _display_ order
    std::list<Task*> m_encodeQueue;     // _global_ queue in _encode_ order (contains _ptr_ to encode task)
    std::list<Task*> m_outputQueue;

    std::list<Task*> m_dpb;             // _global_ pool of frames: encoded reference frames + being encoded frames
    std::list<Task*> m_actualDpb;        // reference frames for next frame to encode
    std::list<H265Frame*> m_freeFrames;  // _global_ free frames (origin/recon/reference) pool


    H265BRC *m_brc;
    TAdapQP *m_pAQP;
#if defined(MFX_ENABLE_H265_PAQ)
    TVideoPreanalyzer m_preEnc;
#endif

#if defined(MFX_VA)
    FeiContext *m_FeiCtx;
#endif

    mfxStatus Init_Internal( void );

    // ------SPS, PPS
    mfxStatus SetProfileLevel();
    mfxStatus SetVPS();
    mfxStatus SetSPS();
    mfxStatus SetPPS();
    mfxStatus SetSlice(H265Slice *slice, Ipp32u curr_slice, H265Frame* currentFrame);

    // ------ _global_ stages of Input Frame Control
    mfxStatus AcceptFrame(mfxFrameSurface1 *surface, mfxBitstream *mfxBS);
    H265Frame *InsertInputFrame(const mfxFrameSurface1 *surface);
    void ConfigureInputFrame(H265Frame *frame) const;
    void PrepareToEncode(Task *task);
    mfxStatus AddNewOutputTask(int& encIdx);// find next task and free encoder, bind them and return encIdx [-1(not found resources), or 0, ..., N-1]
    void OnEncodingQueried(Task *encoded);
    
    // ------ Ref List Managment
    void CreateRefPicList(Task *task, H265ShortTermRefPicSet *rps);
    void InitShortTermRefPicSet();
    void UpdateDpb(Task *currTask);
    void CleanTaskPool();

    // ------ threading
    static mfxStatus TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes);

    mfxStatus EncSolver(Task* task, volatile Ipp32u* onExitEvent);
    void SyncOnTaskCompleted(Task* task, mfxBitstream* mfxBs, void *pParam);

    // ------- Pre Encode Analysis (lookahead / paq etc)
    mfxStatus PreEncAnalysis(void);
    mfxStatus UpdateAllLambda(Task* task);

    // -------FEI
    void ProcessFrameFEI(Task* task);
    void ProcessFrameFEI_Next(Task* task);
};


// pure functions
namespace H265Enc {
    mfxStatus InitH265VideoParam(const mfxVideoParam *param /* IN */, H265VideoParam *par /* OUT */, const mfxExtCodingOptionHEVC *opts_hevc);
}
#endif // __MFX_H265_ENCODE_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
