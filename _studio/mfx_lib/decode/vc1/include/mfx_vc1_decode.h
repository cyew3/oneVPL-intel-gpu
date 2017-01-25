//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"


#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#ifndef _MFX_VC1_DEC_DECODE_H_
#define _MFX_VC1_DEC_DECODE_H_

#include <deque>
#include <list>
#include <memory>

#include "umc_vc1_video_decoder.h"
#ifdef MFX_VA
#include "umc_vc1_video_decoder_hw.h"
#endif
#ifdef ALLOW_SW_VC1_FALLBACK
#include "umc_vc1_video_decoder_sw.h"
#endif

#include "mfx_umc_alloc_wrapper.h"
#include "umc_vc1_spl_frame_constr.h"
#include "umc_mutex.h"
#include "mfx_task.h"
#include "mfx_critical_error_handler.h"

typedef struct 
{
    mfxU64  pts;
    bool    isOriginal;
}VC1TSDescriptor;

class VideoDECODE;
class MFXVideoDECODEVC1 : public VideoDECODE, public MfxCriticalErrorHandler
{
public:
    enum
    {
        MAX_NUM_STATUS_REPORTS = 32
    };

    struct AsyncSurface
    {
        mfxFrameSurface1 *surface_work;
        mfxFrameSurface1 *surface_out;
        mfxU32            taskID;         // for task ordering
        bool              isFrameSkipped; // for status reporting
    };

    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par);

    MFXVideoDECODEVC1(VideoCORE *core, mfxStatus* mfxSts);
    virtual ~MFXVideoDECODEVC1(void);


    mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual mfxStatus GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts);

    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat);

    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs,
                                       mfxFrameSurface1 *surface_work,
                                       mfxFrameSurface1 **surface_out,
                                       MFX_ENTRY_POINT *pEntryPoint);

    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_disp);
    virtual mfxStatus SetSkipMode(mfxSkipMode mode);
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload);
    // to satisfy internal API
    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out){bs, surface_work, surface_out; return MFX_ERR_UNSUPPORTED;};

    mfxStatus RunThread(mfxFrameSurface1 *surface_work, 
                        mfxFrameSurface1 *surface_disp, 
                        mfxU32 threadNumber, 
                        mfxU32 taskID);


protected:


    static mfxStatus SetAllocRequestInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus SetAllocRequestExternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static void      CalculateFramesNumber(mfxFrameAllocRequest *request, mfxVideoParam *par, bool isBufMode);

#ifdef ALLOW_SW_VC1_FALLBACK
    // update Frame Descriptors, copy frames to external memory
    mfxStatus PostProcessFrame(mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_disp);
#endif

    // update Frame Descriptors, copy frames to external memory in case of HW decoder
    mfxStatus PostProcessFrameHW(mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_disp);


    //need for initialization UMC::VC1Decoder
    mfxStatus ConvertMfxToCodecParams(mfxVideoParam *par);

    mfxStatus ConvertMfxBSToMediaDataForFconstr(mfxBitstream    *pBitstream);
    mfxStatus ConvertMfxPlaneToMediaData(mfxFrameSurface1 *surface);

    mfxStatus CheckInput(mfxVideoParam *in);
    mfxStatus CheckForCriticalChanges(mfxVideoParam *in);
    mfxStatus CheckFrameInfo(mfxFrameInfo *info);

    mfxStatus SelfConstructFrame(mfxBitstream *bs);
    mfxStatus SelfDecodeFrame(mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_disp, mfxBitstream *bs);
    mfxStatus ReturnLastFrame(mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_disp);
    
    mfxStatus      FillOutputSurface(mfxFrameSurface1 *surface, UMC::FrameMemID memID);
    mfxStatus      FillOutputSurface(mfxFrameSurface1 *surface);
    void           FillMFXDataOutputSurface(mfxFrameSurface1 *surface);

    mfxStatus      GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index);
    


    UMC::FrameMemID GetDispMemID();

    void            PrepareMediaIn(void);
    static bool     IsHWSupported(VideoCORE *pCore, mfxVideoParam *par);

    // to correspond Opaque memory type
    mfxStatus UpdateAllocRequest(mfxVideoParam *par, 
                                mfxFrameAllocRequest *request, 
                                mfxExtOpaqueSurfaceAlloc **pOpaqAlloc,
                                bool &Mapping,
                                bool &Polar);

    // frame buffering 
    mfxStatus           IsDisplayFrameReady(mfxFrameSurface1 **surface_disp);

    mfxStatus GetStatusReport();
    mfxStatus ProcessSkippedFrame();


    // for Opaque processing
    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *surface);

    static bool         IsBufferMode(VideoCORE *pCore, mfxVideoParam *par);

    bool                IsStatusReportEnable();
    bool                FrameStartCodePresence();

#ifdef MFX_VA_WIN
    bool                NeedToGetStatus(UMC::VC1FrameDescriptor *pCurrDescriptor);
#endif

    void SetFrameOrder(mfx_UMC_FrameAllocator* pFrameAlloc, mfxVideoParam* par, bool isLast, VC1TSDescriptor tsd, bool isSamePolar);
    void FillVideoSignalInfo(mfxExtVideoSignalInfo *pVideoSignal);

    static const        mfxU16 disp_queue_size = 2; // looks enough for Linux now and disable on Windows.


    UMC::BaseCodecParams*      m_VideoParams;
    UMC::MediaData*            m_pInternMediaDataIn; //should be zero in case of Last frame
    UMC::VideoData             m_InternMediaDataOut;
    UMC::MediaData             m_FrameConstrData;

    mfx_UMC_MemAllocator       m_MemoryAllocator;
    // TBD
    std::auto_ptr<mfx_UMC_FrameAllocator>    m_pFrameAlloc;
    std::auto_ptr<UMC::VC1VideoDecoder>      m_pVC1VideoDecoder;

    UMC::vc1_frame_constructor*     m_frame_constructor;
    Ipp8u*                          m_pReadBuffer;
    UMC::MemID                      m_RBufID;
    Ipp32u                          m_BufSize;

    UMC::MediaDataEx::_MediaDataEx*  m_pStCodes;
    UMC::MemID                       m_stCodesID;

    mfxVideoParam                    m_par;
    mfxVideoParam                    m_Initpar;
    Ipp32u                           m_FrameSize;
    bool                             m_bIsInit; // need for sm profile - construct frame

    UMC::VC1FrameConstrInfo          m_FCInfo;
    VideoCORE*                       m_pCore;
    bool                             m_bIsNeedToProcFrame;
    bool                             m_bIsDecInit;
    bool                             m_bIsSWD3D;
    bool                             m_bIsSamePolar;
    bool                             m_bIsDecodeOrder;
    std::deque<UMC::FrameMemID>      m_qMemID;
    std::deque<UMC::FrameMemID>      m_qSyncMemID;
    std::deque<VC1TSDescriptor>      m_qTS;
    
    // for pts in bitstream processing  
    std::deque<mfxU64>               m_qBSTS;

    mfxFrameAllocResponse            m_response;
    mfxFrameAllocResponse            m_response_op;

    Ipp32u                           m_SHSize;
    mfxU8                            m_pSaveBytes[4];  // 4 bytes enough 
    mfxU32                           m_SaveBytesSize;
    mfxBitstream                     m_sbs;
    mfxU32                           m_CurrentBufFrame;
    std::vector<mfxFrameSurface1*>   m_DisplayList;
    std::vector<mfxFrameSurface1*>   m_DisplayListAsync;
    bool                             m_bTakeBufferedFrame;
    bool                             m_bIsBuffering;
    bool                             m_isSWPlatform;
    mfxU32                           m_CurrentTask;
    mfxU32                           m_WaitedTask;

    mfxU32                           m_BufOffset;

    // to synchronize async and sync parts
    UMC::Mutex                       m_guard;

#ifdef MFX_VA_WIN
    // for StatusReporting
    DXVA_Status_VC1                  m_pStatusReport[MAX_NUM_STATUS_REPORTS];
    std::list<DXVA_Status_VC1>       m_pStatusQueue;
#endif

    mfxU32                           m_ProcessedFrames;
    mfxU32                           m_SubmitFrame;
    bool                             m_bIsFirstField;

    bool                             m_IsOpaq;
    mfxFrameSurface1                *m_pPrevOutSurface; // to process skipped frames through coping 

    std::vector<Ipp8u>                m_RawSeq;
    mfxU64                            m_ext_dur;

    mfxExtOpaqueSurfaceAlloc          m_AlloExtBuffer;

    bool                              m_bStsReport;
    mfxU32                            m_NumberOfQueries;
    static const mfxU32               NumOfSeqQuery = 32;
    bool                              m_bPTSTaken;
    
private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    MFXVideoDECODEVC1(const MFXVideoDECODEVC1 &);
    MFXVideoDECODEVC1 & operator = (const MFXVideoDECODEVC1 &);
};

extern mfxStatus __CDECL VC1DECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);

#endif //MFX_ENABLE_VC1_VIDEO_DECODE
#endif

