//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2015 Intel Corporation. All Rights Reserved.
//

#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else

#define __stdcall

#endif // #if defined(_WIN32) || defined(_WIN64)

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "transcode_model_reference.h"

class TranscodeModelAdvanced;
typedef mfxU32 (TranscodeModelAdvanced::*PROUTINE_CALLBACK_M1)(void *pParam);
typedef mfxU32 (__stdcall * PTHREAD_CALLBACK)(void *pParam);

struct sThreadParam
{
    TranscodeModelAdvanced       *pClass;
    PROUTINE_CALLBACK_M1   pMethod;
    void                  *pParam;

};


class TranscodeModelAdvanced:TranscodeModelReference
{
public:
    TranscodeModelAdvanced(AppParam& params);
    ~TranscodeModelAdvanced(void);
    
    mfxStatus Init( AppParam& params );
    mfxStatus Run(void);
    mfxStatus Close( void );

private:
    // event helper function
    void SetEvent(bool & eventFlag);

    // [thread functions]
    mfxU32 BSReaderRoutine(void *pParam);
    mfxU32 DecodeRoutine  (void *pParam);
    mfxU32 VPPRoutine     (void *pParam);
    mfxU32 EncodeRoutine  (void *pParam);
    mfxU32 BSWriterRoutine(void *pParam);
    // sync operations
    mfxU32 SyncOperationDecodeRoutine (void *pParam);
    mfxU32 SyncOperationVPPRoutine (void *pParam);    
    // mfxU32 SyncOperationEncodeRoutine (void *pParam);
    // this function us joined with BSWriterRoutine
    
    // [static wrapper for thread functions]
    static mfxU32 __stdcall WrapperBSReaderRoutine(void *pParam);
    static mfxU32 __stdcall WrapperDecodeRoutine  (void *pParam);
    static mfxU32 __stdcall WrapperVPPRoutine     (void *pParam);
    static mfxU32 __stdcall WrapperEncodeRoutine  (void *pParam);
    static mfxU32 __stdcall WrapperBSWriterRoutine(void *pParam);
    // syncronization
    static mfxU32 __stdcall WrapperSyncOperationDecodeRoutine(void *pParam);
    static mfxU32 __stdcall WrapperSyncOperationVPPRoutine(void *pParam);
    // static mfxU32 __stdcall WrapperSyncOperationEncodeRoutine(void *pParam);
    // this function us joined with WrapperBSWriterRoutine

    // transcoding pipeline specific for component-to-component connection
    typedef std::list<mfxFrameSurfaceEx> Surface1ExList;
    Surface1ExList  m_dec2vppSurfExPool;// pool of decoded frames "prepared" for vpp processing
    std::mutex      m_dec2vpp_mtx;

    Surface1ExList  m_vpp2encSurfExPool;// pool of vpp processed frames "prepared" for encoding
    std::mutex      m_vpp2enc_mtx;

    Surface1ExList  m_dec2encSurfExPool;// pool of decoded frames "prepared" for encoding
    std::mutex      m_dec2enc_mtx;

    typedef std::list<mfxBitstreamEx*> BitstreamExList;
    BitstreamExList    m_outBSPool;     // pool of encoded frames prepared for writting down
    std::auto_ptr<BitstreamExManager>    m_pOutBSManager;
    std::mutex      m_bs_out_mtx;
    std::mutex      m_bs_in_mtx;
    
    // events
    std::mutex m_event_mtx;
    std::condition_variable m_event_cnd;

    // [BSReader]
    bool m_bsr_response;
    bool m_bsr_response_final;

    // [decode]
    bool m_dec_request;
    bool m_dec_response;
    bool m_dec_response_final;
    // [vpp]
    bool m_vpp_request;
    bool m_vpp_response;
    bool m_vpp_response_final;
    // [encode]
    bool m_enc_request;
    bool m_enc_response;
    bool m_enc_response_final;
    // [BSWriter]
    bool m_bswrt_response;
    //// [error]
    bool m_global_err;
    // [SyncOp Decode]
    bool m_syncop_dec_request;
    bool m_syncop_dec_response;
    bool m_syncop_dec_response_final;
    // [SyncOp VPP]
    bool m_syncop_vpp_request;
    bool m_syncop_vpp_response;
    bool m_syncop_vpp_response_final;

    // initialization
    bool m_bInited;

    // async depth
    mfxU16 m_asyncDepth;

    const std::chrono::milliseconds wait_interval;

};

/* EOF */