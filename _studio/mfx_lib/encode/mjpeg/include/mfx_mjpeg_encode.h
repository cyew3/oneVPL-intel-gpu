//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)

#ifndef _MFX_MPJEG_ENC_ENCODE_H_
#define _MFX_MPJEG_ENC_ENCODE_H_

#include "mfx_common_int.h"

#include "mfxvideo++int.h"
#include "mfxvideo.h"

#include "umc_mjpeg_video_encoder.h"
#include "umc_mutex.h"

#include <queue>

class MJPEGEncodeTask
{
public:
    // Default constructor
    MJPEGEncodeTask(void);
    // Destructor
   ~MJPEGEncodeTask(void);

    // Initialize the task object
    mfxStatus Initialize(UMC::VideoEncoderParams* params);

    // Add a picture to the task
    mfxStatus AddSource(mfxFrameSurface1* surface, mfxFrameInfo* frameInfo, bool useAuxInput);

    // Calculate number of pieces to split the task between threads
    mfxU32    CalculateNumPieces(mfxFrameSurface1* surface, mfxFrameInfo* frameInfo);

    // Get the number of pictures collected
    mfxU32    NumPicsCollected(void);

    // Get the number of pieces collected
    mfxU32    NumPiecesCollected(void);

    // Reset the task, drop all counters
    void      Reset(void);

    mfxEncodeCtrl    *ctrl;
    mfxFrameSurface1 *surface;
    mfxBitstream     *bs;
    mfxFrameSurface1 auxInput;

    mfxU32           m_initialDataLength;

    std::auto_ptr<UMC::MJPEGVideoEncoder> m_pMJPEGVideoEncoder;

    mfxStatus EncodePiece(const mfxU32 threadNumber);

protected:
    // Close the object, release all resources
    void      Close(void);

    UMC::Mutex m_guard;

    mfxU32 encodedPieces;
};

class MFXVideoENCODEMJPEG : public VideoENCODE
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void)
    {
        return MFX_TASK_THREADING_INTRA;
    }

    MFXVideoENCODEMJPEG(VideoCORE *core, mfxStatus *sts);
    virtual ~MFXVideoENCODEMJPEG();
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/) {return MFX_ERR_UNSUPPORTED;}

    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *surface);
    mfxStatus RunThread(MJPEGEncodeTask &task, mfxU32 threadNumber, mfxU32 callNumber);

protected:

    VideoCORE *m_core;

    mfxVideoParamWrapper    m_vFirstParam;
    mfxVideoParamWrapper    m_vParam;
    mfxFrameParam           m_mfxFrameParam;

    mfxFrameAllocResponse m_response;
    //mfxFrameAllocResponse m_response_alien;

    // Free tasks queue guard (if SW is used)
    UMC::Mutex m_guard;
    // Free tasks queue (if SW is used)
    std::queue<MJPEGEncodeTask *> m_freeTasks;
    // Count of created tasks (if SW is used)
    mfxU16  m_tasksCount;

    MJPEGEncodeTask *pLastTask;

    std::auto_ptr<UMC::MJPEGEncoderParams> m_pUmcVideoParams;

    mfxU32  m_totalBits;
    mfxU32  m_frameCountSync;   // counter for sync. part
    mfxU32  m_frameCount;       // counter for Async. part
    mfxU32  m_encodedFrames;

    bool m_useAuxInput;
    //bool m_useSysOpaq;
    //bool m_useVideoOpaq;
    bool m_isOpaque;
    bool m_isInitialized;

    mfxExtJPEGQuantTables    m_checkedJpegQT;
    mfxExtJPEGHuffmanTables  m_checkedJpegHT;
    mfxExtOpaqueSurfaceAlloc m_checkedOpaqAllocReq;
    mfxExtBuffer*            m_pCheckedExt[3];

    //
    // Asynchronous processing functions
    //

    static
    mfxStatus MJPEGENCODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static
    mfxStatus MJPEGENCODECompleteProc(void *pState, void *pParam, mfxStatus taskRes);
};

#endif //_MFX_MPJEG_ENC_ENCODE_H_
#endif // MFX_ENABLE_MJPEG_VIDEO_ENCODE