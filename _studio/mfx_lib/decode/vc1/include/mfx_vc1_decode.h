//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"


#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#ifndef _MFX_VC1_DEC_DECODE_H_
#define _MFX_VC1_DEC_DECODE_H_

#include <deque>
#include <list>
#include <memory>

#include "umc_vc1_video_decoder.h"
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

class MFXVC1VideoDecoder : public UMC::VC1VideoDecoder
{
public:
    MFXVC1VideoDecoder();
    //virtual UMC::Status VC1DecodeFrame (UMC::VC1VideoDecoder* pDec, UMC::MediaData* in, UMC::VideoData* out_data);
    virtual UMC::FrameMemID     ProcessQueuesForNextFrame(bool& isSkip, mfxU16& Corrupted);
    UMC::FrameMemID             GetDisplayIndex(bool isDecodeOrder, bool isSamePolarSurf);
    UMC::FrameMemID             GetSkippedIndex(bool isSW, bool isIn = true);

    virtual UMC::FrameMemID     GetLastDisplayIndex();
    virtual UMC::Status         SetRMSurface();
    virtual bool                CanFillOneMoreTask();
    virtual UMC::FrameMemID     GetAsyncLastDisplayIndex() {return -1;}

    void GetDecodeStat(mfxDecodeStat *stat);
    void SetFrameOrder(mfx_UMC_FrameAllocator* pFrameAlloc, mfxVideoParam* par, bool isLast, VC1TSDescriptor tsd, bool isSamePolar);
    void UnlockSurfaces();
    bool IsFrameSkipped();

    void FillVideoSignalInfo(mfxExtVideoSignalInfo *pVideoSignal);
    void SetCorrupted(UMC::VC1FrameDescriptor *pCurrDescriptor, mfxU16& Corrupted);

    

protected:
    friend class MFXVideoDECODEVC1;
    virtual UMC::Status GetAndProcessPerformedDS(UMC::MediaData* in, UMC::VideoData* out_data, UMC::VC1FrameDescriptor **pPerfDescriptor);
    virtual UMC::Status ProcessPrevFrame(UMC::VC1FrameDescriptor *pCurrDescriptor, UMC::MediaData* in, UMC::VideoData* out_dat);
    UMC::VC1FrameDescriptor* m_pDescrToDisplay;
    mfxU32                   m_frameOrder;
    UMC::FrameMemID               m_RMIndexToFree;
    UMC::FrameMemID               m_CurrIndexToFree;
};
class MFXVC1VideoDecoderHW : public MFXVC1VideoDecoder
{
public:
    virtual UMC::FrameMemID     ProcessQueuesForNextFrame(bool& isSkip, mfxU16& Corrupted);
    //virtual UMC::FrameMemID     GetLastDisplayIndex();
    virtual UMC::Status         SetRMSurface();
    //virtual UMC::FrameMemID     GetAsyncLastDisplayIndex();
    // only 1 thread for HW
    //virtual bool                CanFillOneMoreTask();
};

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
        //bool              isLastFrame;
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
                        mfxU32 taskID,
                        bool isSkip);


protected:


    static mfxStatus SetAllocRequestInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus SetAllocRequestExternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static void      CalculateFramesNumber(bool isSW, mfxFrameAllocRequest *request, mfxVideoParam *par, bool isBufMode);

    // update Frame Descriptors, copy frames to external memory
    mfxStatus PostProcessFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_disp);

    // update Frame Descriptors, copy frames to external memory in case of HW decoder
    mfxStatus PostProcessFrameHW(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_disp);


    //need for initialization UMC::VC1Decoder
    mfxStatus ConvertMfxToCodecParams(mfxVideoParam *par);
    //Support of Mfx::GetVideoParam
    mfxStatus ConvertCodecParamsToMfx(mfxVideoParam *par);
     //Support of Mfx::GetFrameParam
    mfxStatus ConvertCodecParamsToMfxFrameParams(mfxFrameParam *par);



    mfxStatus ConvertMfxBSToMediaData(mfxBitstream    *pBitstream);
    mfxStatus ConvertMfxBSToMediaDataForFconstr(mfxBitstream    *pBitstream);
    mfxStatus ConvertMediaDataToMfxBS(mfxBitstream    *pBitstream);
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
    mfxStatus           IsDisplayFrameReady(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_disp);

    mfxStatus GetStatusReport(mfxFrameSurface1 *surface_disp);
    mfxStatus ProcessSkippedFrame();


    // for Opaque processing
    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *surface);

    static bool         IsBufferMode(VideoCORE *pCore, mfxVideoParam *par);

    bool                IsStatusReportEnable();
    bool                FrameStartCodePresence();

#ifdef MFX_VA_WIN
    bool                NeedToGetStatus(UMC::VC1FrameDescriptor *pCurrDescriptor);
#endif

    static const        mfxU16 disp_queue_size = 2; // looks enough for Linux now and disable on Windows.


    UMC::BaseCodecParams*      m_VideoParams;
    UMC::MediaData*            m_pInternMediaDataIn; //should be zero in case of Last frame
    UMC::VideoData             m_InternMediaDataOut;
    UMC::MediaData             m_FrameConstrData;

    mfx_UMC_MemAllocator       m_MemoryAllocator;
    // TBD
    std::auto_ptr<mfx_UMC_FrameAllocator>    m_pFrameAlloc;
    std::auto_ptr<MFXVC1VideoDecoder>        m_pVC1VideoDecoder;

    mfxU16                          m_iFrameBCounter;

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

    bool                             m_bIsWMVMS;
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


#if defined (ELK_WORKAROUND)
    mfxFrameAllocResponse            m_fakeresponse;
#endif
    Ipp32u                           m_SHSize;
    mfxU8                            m_pSaveBytes[4];  // 4 bytes enough 
    mfxU32                           m_SaveBytesSize;
    mfxBitstream                     m_sbs;
    mfxU32                           m_CurrentBufFrame;
    std::vector<mfxFrameSurface1*>   m_DisplayList;
    std::vector<mfxFrameSurface1*>   m_DisplayListAsync;
    bool                             m_bNeedToRunAsyncPart;
    bool                             m_bTakeBufferedFrame;
    bool                             m_bIsBuffering;
    bool                             m_isSWPlatform;
    mfxU32                           m_CurrentTask;
    mfxU32                           m_WaitedTask;

    mfxU32                           m_BufOffset;

    // to synchronize async and sync parts
    UMC::Mutex                       m_guard;

    // for StatusReporting
    DXVA_Status_VC1                  m_pStatusReport[MAX_NUM_STATUS_REPORTS];
    std::list<DXVA_Status_VC1>       m_pStatusQueue;

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

