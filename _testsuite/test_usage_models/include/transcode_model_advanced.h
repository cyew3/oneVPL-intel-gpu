//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//

#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
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
    Surface1ExList  m_vpp2encSurfExPool;// pool of vpp processed frames "prepared" for encoding

    Surface1ExList  m_dec2encSurfExPool;// pool of decoded frames "prepared" for encoding
    typedef std::list<mfxBitstreamEx*> BitstreamExList;
    BitstreamExList    m_outBSPool;     // pool of encoded frames prepared for writting down
    std::auto_ptr<BitstreamExManager>    m_pOutBSManager;
    
    // events
    // [BSReader]
    HANDLE m_hEvent_BSR_RESPONSE; 
    HANDLE m_hEvent_BSR_RESPONSE_FINAL; 
    // [decode]
    HANDLE m_hEvent_DEC_REQUEST;
    HANDLE m_hEvent_DEC_RESPONSE;
    HANDLE m_hEvent_DEC_RESPONSE_FINAL;
    // [vpp]
    HANDLE m_hEvent_VPP_REQUEST;
    HANDLE m_hEvent_VPP_RESPONSE;
    HANDLE m_hEvent_VPP_RESPONSE_FINAL;
    // [encode]
    HANDLE m_hEvent_ENC_REQUEST;
    HANDLE m_hEvent_ENC_RESPONSE;
    HANDLE m_hEvent_ENC_RESPONSE_FINAL;
    // [BSWriter]
    HANDLE m_hEvent_BSWRT_RESPONSE;
    //// [error]
    HANDLE m_hEvent_GLOBAL_ERR;
    // [SyncOp Decode]
    HANDLE m_hEvent_SYNCOP_DEC_REQUEST;
    HANDLE m_hEvent_SYNCOP_DEC_RESPONSE;
    HANDLE m_hEvent_SYNCOP_DEC_RESPONSE_FINAL;
    // [SyncOp VPP]
    HANDLE m_hEvent_SYNCOP_VPP_REQUEST;
    HANDLE m_hEvent_SYNCOP_VPP_RESPONSE;
    HANDLE m_hEvent_SYNCOP_VPP_RESPONSE_FINAL;

    // initialisation
    bool m_bInited;

    // async depth
    mfxU16 m_asyncDepth;

};

#endif // #if defined(_WIN32) || defined(_WIN64)

/* EOF */