//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2015 Intel Corporation. All Rights Reserved.
//
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <process.h>
//AYA debug
#include <WinError.h>

#endif // #if defined(_WIN32) || defined(_WIN64)

#include <chrono>

#include "transcode_model_advanced.h"

/* ******************************************************************** */
/*                     Service functions                                */
/* ******************************************************************** */

//bool   IsDecodeSyncOperationRequired( SessionMode mode );
//bool   IsVppSyncOperationRequired( SessionMode mode );

/* ******************************************************************** */
/*               implementation of TranscodeModelReference              */
/* ******************************************************************** */

TranscodeModelAdvanced::TranscodeModelAdvanced(AppParam& param)
    : TranscodeModelReference( param )
    , m_bInited(false)
    , wait_interval(TUM_WAIT_INTERVAL)
{
  //Init( param );

} // TranscodeModelAdvanced::TranscodeModelAdvanced(AppParam& param)


TranscodeModelAdvanced::~TranscodeModelAdvanced(void)
{
    Close();

} // TranscodeModelAdvanced::~TranscodeModelAdvanced()


mfxStatus TranscodeModelAdvanced::Init( AppParam& param )
{
     mfxStatus sts = TranscodeModelReference::Init( param ); 
     MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // events for transcoding 

    m_bsr_response = false;
    m_bsr_response_final = false;

    m_dec_request = false;
    m_dec_response = false;
    m_dec_response_final = false;

    m_enc_request = false;
    m_enc_response = false;
    m_enc_response_final = false;

    m_bsr_response_final = false;

    // error event
    m_global_err = false;

    // in case of other mfx_session usage models wee need external synchronisation
    // [SyncOp Decode]
    if( IsDecodeSyncOperationRequired( m_sessionMode ) )
    {
        m_syncop_dec_request = false;
        m_syncop_dec_response = false;
        m_syncop_dec_response_final = false;
    }
    if( IsVPPEnable() )
    {
        m_vpp_request = false;
        m_vpp_response = false;
        m_vpp_response_final = false;

        // [SyncOp VPP]
        if( IsVppSyncOperationRequired( m_sessionMode ) )
        {
            m_syncop_vpp_request = false;
            m_syncop_vpp_response = false;
            m_syncop_vpp_response_final = false;
        }
    }

    // async depth take into account
    m_asyncDepth = param.asyncDepth;
    
    m_pOutBSManager.reset(new BitstreamExManager(m_asyncDepth));
    //

    m_bInited = true;

    return sts;

} // mfxStatus TranscodeModelAdvanced::Init( AppParam& param )


mfxStatus TranscodeModelAdvanced::Close( void )
{
    if( m_bInited )
    {
        // reset
        m_bInited = false;
    }

    return MFX_ERR_NONE; //ignore any error

} // mfxStatus TranscodeModelAdvanced::Close( void )


mfxStatus TranscodeModelAdvanced::Run()
{
    mfxStatus sts = MFX_ERR_NONE;

    const int THREADS_COUNT = 7;
    int threadCount = 0;

    PROUTINE_CALLBACK_M1 routineList[THREADS_COUNT];
    routineList[0] = &TranscodeModelAdvanced::BSReaderRoutine;
    routineList[1] = &TranscodeModelAdvanced::DecodeRoutine;    
    routineList[2] = &TranscodeModelAdvanced::EncodeRoutine;
    routineList[3] = &TranscodeModelAdvanced::BSWriterRoutine;

    PTHREAD_CALLBACK threadFuncList[THREADS_COUNT];
    threadFuncList[0] = &TranscodeModelAdvanced::WrapperBSReaderRoutine;
    threadFuncList[1] = &TranscodeModelAdvanced::WrapperDecodeRoutine;    
    threadFuncList[2] = &TranscodeModelAdvanced::WrapperEncodeRoutine;
    threadFuncList[3] = &TranscodeModelAdvanced::WrapperBSWriterRoutine;

    threadCount = 4;

    if( IsDecodeSyncOperationRequired( m_sessionMode ) )
    {
        routineList[threadCount]    = &TranscodeModelAdvanced::SyncOperationDecodeRoutine;
        threadFuncList[threadCount] = &TranscodeModelAdvanced::WrapperSyncOperationDecodeRoutine;
        threadCount++;
    }

    if( IsVPPEnable() )
    {
        routineList[threadCount]    = &TranscodeModelAdvanced::VPPRoutine;
        threadFuncList[threadCount] = &TranscodeModelAdvanced::WrapperVPPRoutine;
        threadCount++;

        if( IsVppSyncOperationRequired( m_sessionMode ) )
        {
            routineList[threadCount]    = &TranscodeModelAdvanced::SyncOperationVPPRoutine;
            threadFuncList[threadCount] = &TranscodeModelAdvanced::WrapperSyncOperationVPPRoutine;
            threadCount++;
        }
    }
    
    sThreadParam threadParamList[THREADS_COUNT];       

    std::thread hThread[THREADS_COUNT];
    for (int threadIndx = 0; threadIndx < threadCount; threadIndx++)
    {
        threadParamList[threadIndx].pClass  = this;
        threadParamList[threadIndx].pMethod = routineList[ threadIndx ];
        threadParamList[threadIndx].pParam  = NULL;

        hThread[threadIndx] = std::thread(threadFuncList[threadIndx], &threadParamList[threadIndx]);
    }

    // start processing
    SetEvent(m_bsr_response);
    
    for (int threadIndx = 0; threadIndx < threadCount; threadIndx++)
    {
        hThread[threadIndx].join();
    }    

    return sts;

} // mfxStatus TranscodeModelAdvanced::Run()

void TranscodeModelAdvanced::SetEvent(bool & eventFlag)
{
    {
        std::unique_lock<std::mutex> locker(m_event_mtx);
        eventFlag = true;
    }
    m_event_cnd.notify_all();
}


/* ******************************************************************** */
/*     implementation of Test Usage Model #1 thread functions           */
/* ******************************************************************** */

mfxU32 TranscodeModelAdvanced::BSReaderRoutine(void *pParam)
{
    pParam;// warning skip

    mfxStatus mfxSts;

    printf("\nBSReaderRoutine: \tstart");

    for(;;)
    {
        bool local_dec_request = false;
        bool local_global_err = false;
        {
            std::unique_lock<std::mutex> lk(m_event_mtx);
            m_event_cnd.wait_for(lk, wait_interval,
                [&](){ return m_dec_request
                           || m_global_err; });

            if (m_dec_request)
                std::swap(local_dec_request, m_dec_request);
            else if (m_global_err)
                std::swap(local_global_err, m_global_err);

        }

        if (local_dec_request)  // m_hEvent_DEC_REQUEST
        {
            // main operation here
            {
                std::unique_lock<std::mutex> lk(m_bs_in_mtx);
                mfxSts = ReadOneFrame();  //critical section???
            }
            if(MFX_ERR_NONE == mfxSts)
            {
                SetEvent(m_bsr_response);
                //SetEvent( m_hEvent_BSR_RESPONSE );
            }
            else if ( MFX_ERR_MORE_DATA == mfxSts )
            {
                SetEvent(m_bsr_response_final);

                printf("\n\nBSReaderRoutine: \texit with OK");//EXIT from thread
                return TUM_OK_STS;//OK
            }
            else
            {   
                SetEvent(m_global_err);
                RETURN_ON_ERROR( _T("\nBSReaderRoutine: exit with FAIL_1"), TUM_ERR_STS);
            }
        }
        else if (local_global_err)
        {
            RETURN_ON_ERROR(_T("\nBSReaderRoutine: exit with GLOBAL_ERR"), TUM_ERR_STS);
        }
        else
        {
            SetEvent(m_global_err);
            RETURN_ON_ERROR( _T("\nBSReaderRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }
    }

    //RETURN_ON_ERROR( _T("\nBSREADER: exit with UKNOWN ERROR\n"), TUM_ERR_STS);

} // mfxU32 __stdcall TranscodeModelAdvanced::BSReaderRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::DecodeRoutine(void *pParam)
{
    pParam; // warninh disable

    // state we interested in depends on usage model
    bool & request_state = 
        IsDecodeSyncOperationRequired(m_sessionMode)
        ? m_syncop_dec_request
        : (IsVPPEnable() 
          ? m_vpp_request 
          : m_enc_request);

    // send list
    bool & eventRequest = m_dec_request;
    bool & eventResponse = m_dec_response;
    bool & eventResponseFinal = m_dec_response_final;

    bool bRequestEnable = true;
    TUMSurfacesManager* pSurfMngr;
    Surface1ExList*     pConnectPool;
    std::mutex *        p_pool_mtx;

    if( IsVPPEnable() )
    {
        pSurfMngr = &m_dec2vppSurfManager;
        pConnectPool = &m_dec2vppSurfExPool;
        p_pool_mtx = &m_dec2vpp_mtx;
    }
    else
    {
        pSurfMngr = &m_dec2encSurfManager;
        pConnectPool = &m_dec2encSurfExPool;
        p_pool_mtx = &m_dec2enc_mtx;
    }

    printf("\nDecodeRoutine: \t\tstart");

    for(;;)
    {
        bool local_bsr_response = false;
        bool local_bsr_response_final = false;
        bool local_request = false;
        bool local_global_err = false;
        {
            std::unique_lock<std::mutex> lk(m_event_mtx);
            m_event_cnd.wait_for(lk, wait_interval,
                [&](){ return m_bsr_response 
                           || m_bsr_response_final
                           || request_state
                           || m_global_err; });

            if (m_bsr_response) 
                std::swap(local_bsr_response, m_bsr_response);
            else if (m_bsr_response_final) 
                std::swap(local_bsr_response_final, m_bsr_response_final);
            else if (request_state) 
                std::swap(local_request, request_state);
            else if (m_global_err) 
                std::swap(local_global_err, m_global_err);

        }

        if (local_bsr_response) // m_hEvent_BSR_RESPONSE
        {
            mfxFrameSurfaceEx decSurfaceEx = {};
            mfxStatus mfxSts;

            {
                std::unique_lock<std::mutex> lk1(m_bs_in_mtx);
                mfxBitstream *pBS = GetSrcPtr(); //critical section???

                std::unique_lock<std::mutex> lk2(*p_pool_mtx);

                mfxSts = DecodeOneFrame(pBS, pSurfMngr, &decSurfaceEx);
            }
            //-----------------------------------------------------------------
            if( MFX_ERR_MORE_DATA == mfxSts )
            {
                if( bRequestEnable )
                {
                    SetEvent(eventRequest);
                }
                else
                {          
                    // to get buffered VPP or ENC frames
                    decSurfaceEx.pSurface = NULL;
                    decSurfaceEx.syncp = 0;
                    {
                        std::unique_lock<std::mutex> lk(*p_pool_mtx);
                        pConnectPool->push_back(decSurfaceEx);// critical section???
                    }

                    SetEvent(eventResponseFinal);

                    printf("\nDecodeRoutine: \t\texit with OK");
                    return TUM_OK_STS; // EXIT from thread
                }            
            }
            else if (MFX_ERR_NONE == mfxSts) 
            {
                {
                    std::unique_lock<std::mutex> lk(*p_pool_mtx);
                    pConnectPool->push_back(decSurfaceEx);// critical section???
                }

                SetEvent(eventResponse);
            }
            else
            {
                SetEvent(m_global_err);
                RETURN_ON_ERROR( _T("\nDecodeRoutine: exit with FAIL_1"), TUM_ERR_STS);
            }
            //-----------------------------------------------------------------
        }
        else if (local_bsr_response_final) // m_hEvent_BSR_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent(m_bsr_response);
        }
        else if (local_request) // m_hEvent_VPP_REQUEST
        {
            SetEvent(m_bsr_response);
        }
        else if (local_global_err) // m_hEvent_GLOBAL_ERR
        {
            RETURN_ON_ERROR( _T("\nDecodeRoutine: exit with GLOBAL_ERR\n"), TUM_ERR_STS);
        }
        else
        {
            SetEvent(m_global_err);
            RETURN_ON_ERROR( _T("\nDecodeRoutine: exit with UNKNOWN ERR\n"), TUM_ERR_STS);
        }
    }

    //RETURN_ON_ERROR( _T("\nDecodeRoutine: exit with FAIL_3\n"), TUM_ERR_STS);

} // mfxU32 __stdcall TranscodeModelAdvanced::DecodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::VPPRoutine(void *pParam)
{
    pParam;// warning disable

    // wait list
    bool & response_state = 
        IsDecodeSyncOperationRequired(m_sessionMode)
        ? m_syncop_dec_response
        : m_dec_response;

    bool & response_final_state =
        IsDecodeSyncOperationRequired(m_sessionMode)
        ? m_syncop_dec_response_final
        : m_dec_response_final;

    bool & request_state =
        IsVppSyncOperationRequired(m_sessionMode)
        ? m_syncop_vpp_request
        : m_enc_request;

    // send list
    bool & eventRequest = m_vpp_request; // TODO: m_syncop_vpp_response!
    bool & eventResponse = m_vpp_response;
    bool & eventResponseFinal = m_vpp_response_final;

    bool bRequestEnable = true;
    mfxFrameSurfaceEx zeroSurfaceEx = {NULL, 0};

    printf("\nVPPRoutine: \t\tstart");

    for(;;)
    {
        bool local_response = false;
        bool local_response_final = false;
        bool local_request = false;
        bool local_global_err = false;
        bool waitStatus;
        {
            std::unique_lock<std::mutex> lk(m_event_mtx);
            waitStatus = m_event_cnd.wait_for(lk, wait_interval,
                [&](){ return response_state
                           || response_final_state
                           || request_state
                           || m_global_err; });

            if (response_state)
                std::swap(local_response, response_state);
            else if (response_final_state)
                std::swap(local_response_final, response_final_state);
            else if (request_state)
                std::swap(local_request, request_state);
            else if (m_global_err)
                std::swap(local_global_err, m_global_err);

        }

        if (local_response) // m_hEvent_DEC_RESPONSE
        {
            mfxFrameSurfaceEx outputSurfaceEx = {};
            mfxStatus mfxSts;
            {
                std::unique_lock<std::mutex> lk1(m_dec2vpp_mtx);
                mfxFrameSurfaceEx inputSurfaceEx = (m_dec2vppSurfExPool.size()) ? m_dec2vppSurfExPool.front() : zeroSurfaceEx;//critical section            

                std::unique_lock<std::mutex> lk2(m_vpp2enc_mtx);
                mfxSts = VPPOneFrame(inputSurfaceEx.pSurface, &m_vpp2encSurfManager, &outputSurfaceEx);

                // after VPPOneFrame() we should erase first element. real data is kept by mfx_session
                if (m_dec2vppSurfExPool.size() > 0)
                {
                    m_dec2vppSurfExPool.pop_front();
                }
            }

            //-----------------------------------------------------------------
            if( MFX_ERR_MORE_DATA == mfxSts )
            {
                if( bRequestEnable )
                {
                    SetEvent(eventRequest);
                }
                else
                {          
                    // to get buffered VPP or ENC frames
                    outputSurfaceEx.pSurface = NULL;
                    outputSurfaceEx.syncp = 0;

                    {
                        std::unique_lock<std::mutex> lk(m_vpp2enc_mtx);
                        m_vpp2encSurfExPool.push_back(outputSurfaceEx);// critical section???
                    }

                    SetEvent(eventResponseFinal);

                    printf("\nVPPRoutine: \t\texit with OK");
                    return TUM_OK_STS; // EXIT from thread
                }            
            }
            else if (MFX_ERR_NONE == mfxSts) 
            {
                {
                    std::unique_lock<std::mutex> lk(m_vpp2enc_mtx);
                    m_vpp2encSurfExPool.push_back(outputSurfaceEx);// critical section???
                }
                SetEvent(eventResponse);
            }
            else
            {   
                SetEvent(m_global_err);
                RETURN_ON_ERROR( _T("\nVPPRoutine: exit with FAIL_1"), TUM_ERR_STS);                
            }
            //-----------------------------------------------------------------
        }
        else if (local_response_final) // m_hEvent_DEC_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent(response_state);
        }
        else if (local_request) // m_hEvent_ENC_REQUEST
        {
            if( m_dec2vppSurfExPool.size() > 0 ) // we have frames in input frames pool
            {
                SetEvent(response_state);
            }
            else if ( bRequestEnable ) // we require frames from previous (VPP) component
            {
                SetEvent(eventRequest);
            }
            else // we start "last frame" processing
            {
                SetEvent(response_state);
            }
        }
        else if (local_global_err) // m_hEvent_GLOBAL_ERR
        {
            RETURN_ON_ERROR( _T("\nVPPRoutine: exit with GLOBAL ERR"), TUM_ERR_STS);
        }
        else if (!waitStatus) // timeout
        {
            SetEvent(m_global_err);
            RETURN_ON_ERROR( _T("\nVPPRoutine: exit with FAIL TIMEOUT"), TUM_ERR_STS);
        }
        else
        {   
            SetEvent(m_global_err);
            RETURN_ON_ERROR( _T("\nVPPRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }
    }
    
    //RETURN_ON_ERROR( _T("\nVPPRoutine: exit with UNKNOWN_ERR\n"), TUM_ERR_STS);

} // mfxU32 __stdcall TranscodeModelAdvanced::VPPRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::EncodeRoutine(void *pParam)
{
    pParam; // disable

    // wait list

    bool & response_state =
        IsVPPEnable()
        ? (IsVppSyncOperationRequired(m_sessionMode)
          ? m_syncop_vpp_response
          : m_vpp_response)
        : m_dec_response;

    bool & response_final_state =
        IsVPPEnable()
        ? (IsVppSyncOperationRequired(m_sessionMode)
          ? m_syncop_vpp_response_final
          : m_vpp_response_final)
        : m_dec_response_final;



    // send list
    bool & eventRequest = m_enc_request;
    bool & eventResponse = m_enc_response;
    bool & eventResponseFinal = m_enc_response_final;

    bool bRequestEnable = true;
    mfxFrameSurfaceEx zeroSurfaceEx = {NULL, 0};

    Surface1ExList*     pConnectPool;
    std::mutex*         p_pool_mtx;

    if( IsVPPEnable() )
    {
        pConnectPool = &m_vpp2encSurfExPool;
        p_pool_mtx = &m_vpp2enc_mtx;
    }
    else
    {
        pConnectPool = &m_dec2encSurfExPool;
        p_pool_mtx = &m_dec2enc_mtx;
    }
    printf("\nEncodeRoutine: \t\tstart");

    for(;;)
    {
        bool local_response = false;
        bool local_response_final = false;
        bool local_bswrt_response = false;
        bool local_global_err = false;
        {
            std::unique_lock<std::mutex> lk(m_event_mtx);
            m_event_cnd.wait_for(lk, wait_interval,
                [&](){ return response_state
                           || response_final_state
                           || m_bswrt_response
                           || m_global_err; });

            if (response_state)
                std::swap(local_response, response_state);
            else if (response_final_state)
                std::swap(local_response_final, response_final_state);
            else if (m_bswrt_response)
                std::swap(local_bswrt_response, m_bswrt_response);
            else if (m_global_err)
                std::swap(local_global_err, m_global_err);

        }

        if (local_response) // m_hEvent_VPP_RESPONSE
        {
            std::unique_lock<std::mutex> lk1(*p_pool_mtx);
            mfxFrameSurfaceEx inputSurfaceEx = (pConnectPool->size() > 0) ? pConnectPool->front() : zeroSurfaceEx;//critical section???

            std::unique_lock<std::mutex> lk2(m_bs_out_mtx);
            mfxBitstreamEx* pBSEx = NULL;

            pBSEx = m_pOutBSManager->GetNext();
            // wait for free output bitsream
            while (!pBSEx)
            {
                lk2.unlock();
                MSDK_SLEEP(60);
                lk2.lock();
                pBSEx = m_pOutBSManager->GetNext();
            }
            pBSEx->syncp = 0;

            mfxStatus mfxSts = EncodeOneFrame(inputSurfaceEx.pSurface, pBSEx);

            // after EncodeOneFrame() we should erase first element. real data is kept by mfx_session
            if(pConnectPool->size() > 0 )
            {
                pConnectPool->pop_front();
            }
            lk1.unlock();
            //-----------------------------------------------------------------
            if( MFX_ERR_MORE_DATA == mfxSts )
            {
                // the task in not in Encode queue
                m_pOutBSManager->Release( pBSEx );
                lk2.unlock();

                if( bRequestEnable )
                {
                    SetEvent(eventRequest);
                }
                else
                {
                    SetEvent(eventResponseFinal);

                    printf("\nEncodeRoutine: \t\texit with OK");
                    return TUM_OK_STS; // EXIT from thread
                }            
            }
            else if (MFX_ERR_NONE == mfxSts) 
            {
                m_outBSPool.push_back( pBSEx );// critical section???
                lk2.unlock();

                SetEvent(eventResponse);
            }
            else
            {   
                SetEvent(m_global_err);
                RETURN_ON_ERROR( _T("\nEncodeRoutine: exit with FAIL_1"), TUM_ERR_STS);
            }
            //-----------------------------------------------------------------
        }
        else if (local_response_final) // m_hEvent_VPP_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent(response_state);
        }
        else if (local_bswrt_response) // m_hEvent_BSWRT_REQUEST
        {
            if( pConnectPool->size() > 0 ) // we have frames in input frames pool
            {
                SetEvent(response_state);
            }
            else if ( bRequestEnable ) // we require frames from previous (VPP) component
            {
                SetEvent(eventRequest);
            }
            else // we start "last frame" processing
            {
                SetEvent(response_state);
            }
        }
        else if (local_global_err) // // m_hEvent_GLOBAL_ERR
        {
            RETURN_ON_ERROR( _T("\nEncodeRoutine: exit with GLOBAL ERR"), TUM_ERR_STS);
        }
        else
        {
            SetEvent(m_global_err);
            RETURN_ON_ERROR( _T("\nEncodeRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }
    }
    
    //RETURN_ON_ERROR( _T("\nEncodeRoutine: exit with UNKNOWN_ERR\n"), TUM_ERR_STS);

} // mfxU32 __stdcall TranscodeModelAdvanced::EncodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::SyncOperationDecodeRoutine(void *pParam)
{
    pParam; // disable

    // wait list
    bool & request_state =
        IsVPPEnable()
        ? m_vpp_request
        : m_enc_request;

    // send list
    bool & eventRequest = m_syncop_dec_request;
    bool & eventResponse = m_syncop_dec_response;
    bool & eventResponseFinal = m_syncop_dec_response_final;

    bool bRequestEnable = true;

    mfxFrameSurfaceEx zeroSurfaceEx = {};

    Surface1ExList*    pConnectPool;
    std::mutex*        p_pool_mtx;

    if( IsVPPEnable() )
    {
        pConnectPool = &m_dec2vppSurfExPool;
        p_pool_mtx = &m_dec2vpp_mtx;
    }
    else
    {
        pConnectPool = &m_dec2encSurfExPool;
        p_pool_mtx = &m_dec2enc_mtx;
    }

    printf("\nSyncOpDECRoutine:\tstart");

    for(;;)
    {
        bool local_dec_response = false;
        bool local_dec_response_final = false;
        bool local_request_state = false;
        bool local_global_err = false;
        {
            std::unique_lock<std::mutex> lk(m_event_mtx);
            m_event_cnd.wait_for(lk, wait_interval,
                [&](){ return m_dec_response
                           || m_dec_response_final
                           || request_state
                           || m_global_err; });

            if (m_dec_response)
                std::swap(local_dec_response, m_dec_response);
            else if (m_dec_response_final)
                std::swap(local_dec_response_final, m_dec_response_final);
            else if (request_state)
                std::swap(local_request_state, request_state);
            else if (m_global_err)
                std::swap(local_global_err, m_global_err);

        }

        if (local_dec_response) // m_hEvent_DEC_RESPONSE
        {
            //if( (m_dec2vppSurfExPool.size() != m_asyncDepth) && (bRequestEnable) )
            //{
            //    SetEvent( hEventRequest );
            //}
            ////------------------------------------------------------------------
            //else // normal processing
            {
                mfxFrameSurfaceEx surfaceEx;
                {
                    std::unique_lock<std::mutex> lk(*p_pool_mtx);
                    surfaceEx = (pConnectPool->size()) ? pConnectPool->front() : zeroSurfaceEx;//critical section
                }

                if( 0 == surfaceEx.syncp )
                {
                    if( !bRequestEnable )
                    {
                        SetEvent(eventResponseFinal);

                        printf("\nSyncOpDECRoutine:\texit with OK");
                        return TUM_OK_STS; // EXIT from thread
                    }
                    else
                    {
                        SetEvent(m_global_err);
                        RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine:exit with UNKNOWN ERR"), TUM_ERR_STS);
                    }
                }
                else
                {
                    mfxStatus mfxSts = MFX_ERR_NONE;

                    if( surfaceEx.syncp )
                    {
                        mfxSts = SynchronizeDecode( surfaceEx.syncp );
                    }

                    if (MFX_ERR_NONE == mfxSts) 
                    {
                        SetEvent(eventResponse);
                    }
                    else
                    {
                        SetEvent(m_global_err);
                        RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine: FAIL"), TUM_ERR_STS);
                    }
                }

            }
        }
        else if (local_dec_response_final) // m_hEvent_DEC_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent(m_dec_response);
        }
        else if (local_request_state) // m_hEvent_VPP_REQUEST
        {
            if ( bRequestEnable ) // we require frames from previous component
            {
                SetEvent(eventRequest);
            }
            else // we start "last frame" processing
            {
                SetEvent(m_dec_response);
            }
        }
        else
        {
            SetEvent(m_global_err);
            RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }        
    }

} // mfxU32 TranscodeModelAdvanced::SyncOperationDecodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::SyncOperationVPPRoutine(void *pParam)
{
    pParam; // disable

    // wait list

    // send list
    bool & eventRequest = m_syncop_vpp_request;
    bool & eventResponse = m_syncop_vpp_response;
    bool & eventResponseFinal = m_syncop_vpp_response_final;

    bool bRequestEnable = true;

    mfxFrameSurfaceEx zeroSurfaceEx = {};

    printf("\nSyncOpVPPRoutine: \tstart");

    for(;;)
    {
        bool local_vpp_response = false;
        bool local_vpp_response_final = false;
        bool local_enc_request = false;
        bool local_global_err = false;
        {
            std::unique_lock<std::mutex> lk(m_event_mtx);
            m_event_cnd.wait_for(lk, wait_interval,
                [&](){ return m_vpp_response
                           || m_vpp_response_final
                           || m_enc_request
                           || m_global_err; });

            if (m_vpp_response)
                std::swap(local_vpp_response, m_vpp_response);
            else if (m_vpp_response_final)
                std::swap(local_vpp_response_final, m_vpp_response_final);
            else if (m_enc_request)
                std::swap(local_enc_request, m_enc_request);
            else if (m_global_err)
                std::swap(local_global_err, m_global_err);

        }

        if (local_vpp_response) // m_hEvent_VPP_RESPONSE
        {
            //if( (m_vpp2encSurfExPool.size() != m_asyncDepth) && (bRequestEnable) )
            //{
            //    SetEvent( hEventRequest );
            //}
            ////------------------------------------------------------------------
            //else // normal processing
            {
                mfxFrameSurfaceEx surfaceEx;
                {
                    std::unique_lock<std::mutex> lk(m_vpp2enc_mtx);
                    surfaceEx = (m_vpp2encSurfExPool.size()) ? m_vpp2encSurfExPool.front() : zeroSurfaceEx;//critical section
                }

                if( 0 == surfaceEx.syncp )
                {
                    if( !bRequestEnable )
                    {
                        SetEvent(eventResponseFinal);

                        printf("\nSyncOperationDECRoutine:exit with OK");
                        return TUM_OK_STS; // EXIT from thread
                    }
                    else
                    {
                        SetEvent(m_global_err);
                        RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
                    }
                }
                else
                {
                    mfxStatus mfxSts = MFX_ERR_NONE;

                    if( surfaceEx.syncp )
                    {
                        mfxSts = SynchronizeVPP( surfaceEx.syncp );
                    }

                    if (MFX_ERR_NONE == mfxSts) 
                    {
                        SetEvent(eventResponse);
                    }
                    else
                    {
                        SetEvent(m_global_err);
                        RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine: FAIL"), TUM_ERR_STS);
                    }
                }

            }
        }
        else if (local_vpp_response_final) // m_hEvent_DEC_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent(m_vpp_response);
        }
        else if (local_enc_request) // m_hEvent_VPP_REQUEST
        {
            if ( bRequestEnable ) // we require frames from previous component
            {
                SetEvent(eventRequest);
            }
            else // we start "last frame" processing
            {
                SetEvent(m_vpp_response);
            }
        }
        else
        {
            SetEvent(m_global_err);
            RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }        
    }

} // mfxU32 TranscodeModelAdvanced::SyncOperationDecodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::BSWriterRoutine(void *pParam)
{
    pParam; // warning disable

    bool bRequestEnable = true;

    printf("\nBSWriterRoutine: \tstart");

    for(;;)
    {
        bool local_enc_response = false;
        bool local_enc_response_final = false;
        bool local_global_err = false;
        {
            std::unique_lock<std::mutex> lk(m_event_mtx);
            m_event_cnd.wait_for(lk, wait_interval,
                [&](){ return m_enc_response
                           || m_enc_response_final
                           || m_global_err; });

            if (m_enc_response)
                std::swap(local_enc_response, m_enc_response);
            else if (m_enc_response_final)
                std::swap(local_enc_response_final, m_enc_response_final);
            else if (m_global_err)
                std::swap(local_global_err, m_global_err);

        }

        if (local_enc_response) // m_hEvent_ENC_RESPONSE
        {
            std::unique_lock<std::mutex> lk(m_bs_out_mtx);

            if( 0 == m_outBSPool.size() )
            {
                printf("\nBSWriterRoutine: \texit with OK");
                return TUM_OK_STS; // EXIT from thread
            }
            if( (m_outBSPool.size() != m_asyncDepth) && (bRequestEnable) )
            {
                lk.unlock();
                //printf("\n BSWRT: async_pool not ready yet \n");
                // continue transcode processing wo synchronization
                SetEvent(m_bswrt_response);
            }
            //-----------------------------------------------------------------
            else // normal processing
            {            
                mfxBitstreamEx* pBSEx = m_outBSPool.front(); //critical section???                

                // get result coded stream
                mfxStatus mfxSts = SynchronizeEncode( pBSEx->syncp );            

                if (MFX_ERR_NONE == mfxSts) 
                {
                    WriteOneFrame( &(pBSEx->bitstream) );

                    // we should release captured element/resource. 
                    m_outBSPool.pop_front(); // critical section       
                    m_pOutBSManager->Release( pBSEx );
                    lk.unlock();

                    if( bRequestEnable )
                    {
                        SetEvent(m_bswrt_response);
                    }
                    else
                    {
                        SetEvent(m_enc_response);
                    }
                }
                else
                { 
                    SetEvent(m_global_err);
                    RETURN_ON_ERROR( _T("\nSynchronizeEncode: FAIL"), TUM_ERR_STS);
                }
            }
            //-----------------------------------------------------------------
        }
        else if (local_enc_response_final) // m_hEvent_ENC_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent(m_enc_response);
        }
        else if (local_global_err) // m_hEvent_GLOBAL_ERR
        {
            RETURN_ON_ERROR( _T("\nBSWriterRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }
        else
        {
            SetEvent(m_global_err);
            RETURN_ON_ERROR( _T("\nBSWriterRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }
    }

    //RETURN_ON_ERROR( _T("\nBSWriterRoutine: exit with UNKNOWN_ERR\n"), TUM_ERR_STS);

} // mfxU32 __stdcall TranscodeModelAdvanced::BSWriterRoutine(void *pParam)

/* ************************************************************************************ */
/*                       Wrapper functions for thread functions                         */
/* ************************************************************************************ */

mfxU32 TranscodeModelAdvanced::WrapperBSReaderRoutine(void *pParam)
{
    sThreadParam* pThreadParam = (sThreadParam*)pParam;

    TranscodeModelAdvanced*      pClass     = pThreadParam->pClass;
    PROUTINE_CALLBACK_M1  pMethod    = pThreadParam->pMethod;
    //void*                 pFuncParam = pThreadParam->pParam;

    printf("\nWrapperBSReaderRoutine: start");

    mfxU32 result = (pClass->*pMethod)(pParam);

    printf("\nWrapperBSReaderRoutine: exit with sts = %i ", result);

    return result;

} // mfxU32 TranscodeModelAdvanced::WrapperBSReaderRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::WrapperDecodeRoutine(void *pParam)
{
    sThreadParam* pThreadParam = (sThreadParam*)pParam;

    TranscodeModelAdvanced*      pClass     = pThreadParam->pClass;
    PROUTINE_CALLBACK_M1  pMethod    = pThreadParam->pMethod;
    //void*               pFuncParam = pThreadParam->pParam;

    printf("\nWrapperDecodeRoutine: \tstart");

    mfxU32 result = (pClass->*pMethod)(pParam);

    printf("\nWrapperDecodeRoutine: \texit with sts = %i", result);

    return result;

} // mfxU32 TranscodeModelAdvanced::WrapperDecodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::WrapperVPPRoutine(void *pParam)
{
    sThreadParam* pThreadParam = (sThreadParam*)pParam;

    TranscodeModelAdvanced*      pClass    = pThreadParam->pClass;
    PROUTINE_CALLBACK_M1  pMethod   = pThreadParam->pMethod;
    //void*              pFuncParam = pThreadParam->pParam;

    printf("\nWrapperVPPRoutine: \tstart");

    mfxU32 result = (pClass->*pMethod)(pParam);

    printf("\nWrapperVPPRoutine: \texit with sts = %i", result);

    return result;

} // mfxU32 TranscodeModelAdvanced::WrapperVPPRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::WrapperEncodeRoutine(void *pParam)
{
    sThreadParam* pThreadParam = (sThreadParam*)pParam;

    TranscodeModelAdvanced*      pClass  = pThreadParam->pClass;
    PROUTINE_CALLBACK_M1  pMethod = pThreadParam->pMethod;
    //void*              pFuncParam = pThreadParam->pParam;

    printf("\nWrapperEncodeRoutine: \tstart");

    mfxU32 result = (pClass->*pMethod)(pParam);

    printf("\nWrapperEncodeRoutine: \texit with sts = %i", result);

    return result;

} // mfxU32 TranscodeModelAdvanced::WrapperEncodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::WrapperBSWriterRoutine(void *pParam)
{
    sThreadParam* pThreadParam = (sThreadParam*)pParam;

    TranscodeModelAdvanced*       pClass     = pThreadParam->pClass;
    PROUTINE_CALLBACK_M1   pMethod    = pThreadParam->pMethod;
    //void*                pFuncParam = pThreadParam->pParam;

    printf("\nWrapperBSWriterRoutine: start");

    mfxU32 result = (pClass->*pMethod)(pParam);

    printf("\nWrapperBSWriterRoutine: exit with sts = %i ", result);

    return result;

} // mfxU32 TranscodeModelAdvanced::WrapperBSWriterRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::WrapperSyncOperationDecodeRoutine(void *pParam)
{
    sThreadParam* pThreadParam = (sThreadParam*)pParam;

    TranscodeModelAdvanced*       pClass     = pThreadParam->pClass;
    PROUTINE_CALLBACK_M1   pMethod    = pThreadParam->pMethod;
    //void*                pFuncParam = pThreadParam->pParam;

    printf("\nWrapperSyncOpDECRoutine:start");

    mfxU32 result = (pClass->*pMethod)(pParam);

    printf("\nWrapperSyncOpDECRoutine:exit with sts = %i ", result);

    return result;

} // mfxU32 TranscodeModelAdvanced::WrapperSyncOperationDecodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::WrapperSyncOperationVPPRoutine(void *pParam)
{
    sThreadParam* pThreadParam = (sThreadParam*)pParam;

    TranscodeModelAdvanced*       pClass     = pThreadParam->pClass;
    PROUTINE_CALLBACK_M1   pMethod    = pThreadParam->pMethod;
    //void*                pFuncParam = pThreadParam->pParam;

    printf("\nWrapperSyncOpVPPRoutine:start");

    mfxU32 result = (pClass->*pMethod)(pParam);

    printf("\nWrapperSyncOperationVPPRoutine: exit with sts = %i ", result);

    return result;

} // mfxU32 TranscodeModelAdvanced::WrapperSyncOperationVPPRoutine(void *pParam)

/* EOF */
