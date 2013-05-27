//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//
#include <windows.h>
#include <process.h>
//AYA debug
#include <WinError.h>

#include "transcode_model_advanced.h"

/* ******************************************************************** */
/*                     Service functions                                */
/* ******************************************************************** */

//bool   IsDecodeSyncOperationRequired( SessionMode mode );
//bool   IsVppSyncOperationRequired( SessionMode mode );

/* ******************************************************************** */
/*               implementation of TranscodeModelReference              */
/* ******************************************************************** */

TranscodeModelAdvanced::TranscodeModelAdvanced(AppParam& param): TranscodeModelReference( param ), m_bInited(false)
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

    m_hEvent_BSR_RESPONSE       = CreateEvent(NULL, false, false, NULL);
    m_hEvent_BSR_RESPONSE_FINAL = CreateEvent(NULL, false, false, NULL);

    m_hEvent_DEC_REQUEST = CreateEvent(NULL, false, false, NULL);
    m_hEvent_DEC_RESPONSE = CreateEvent(NULL, false, false, NULL);
    m_hEvent_DEC_RESPONSE_FINAL = CreateEvent(NULL, false, false, NULL);

    m_hEvent_ENC_REQUEST = CreateEvent(NULL, false, false, NULL);
    m_hEvent_ENC_RESPONSE = CreateEvent(NULL, false, false, NULL);
    m_hEvent_ENC_RESPONSE_FINAL = CreateEvent(NULL, false, false, NULL);

    m_hEvent_BSWRT_RESPONSE = CreateEvent(NULL, false, false, NULL);

    // error event
    m_hEvent_GLOBAL_ERR = CreateEvent(NULL, true, false, NULL);//manual reset

    // in case of other mfx_session usage models wee need external synchronisation
    // [SyncOp Decode]
    if( IsDecodeSyncOperationRequired( m_sessionMode ) )
    {
        m_hEvent_SYNCOP_DEC_REQUEST  = CreateEvent(NULL, false, false, NULL);    
        m_hEvent_SYNCOP_DEC_RESPONSE = CreateEvent(NULL, false, false, NULL);    
        m_hEvent_SYNCOP_DEC_RESPONSE_FINAL = CreateEvent(NULL, false, false, NULL);
    }
    if( IsVPPEnable() )
    {
        m_hEvent_VPP_REQUEST = CreateEvent(NULL, false, false, NULL);
        m_hEvent_VPP_RESPONSE = CreateEvent(NULL, false, false, NULL);
        m_hEvent_VPP_RESPONSE_FINAL = CreateEvent(NULL, false, false, NULL);

        // [SyncOp VPP]
        if( IsVppSyncOperationRequired( m_sessionMode ) )
        {
            m_hEvent_SYNCOP_VPP_REQUEST        = CreateEvent(NULL, false, false, NULL);    
            m_hEvent_SYNCOP_VPP_RESPONSE       = CreateEvent(NULL, false, false, NULL);    
            m_hEvent_SYNCOP_VPP_RESPONSE_FINAL = CreateEvent(NULL, false, false, NULL);    
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
        CloseHandle( m_hEvent_BSR_RESPONSE );
        CloseHandle( m_hEvent_BSR_RESPONSE_FINAL );

        CloseHandle( m_hEvent_DEC_REQUEST );
        CloseHandle( m_hEvent_DEC_RESPONSE );
        CloseHandle( m_hEvent_DEC_RESPONSE_FINAL );

        CloseHandle( m_hEvent_ENC_REQUEST );
        CloseHandle( m_hEvent_ENC_RESPONSE );
        CloseHandle( m_hEvent_ENC_RESPONSE_FINAL );

        CloseHandle( m_hEvent_BSWRT_RESPONSE );

        // error event
        CloseHandle( m_hEvent_GLOBAL_ERR );

        if( IsDecodeSyncOperationRequired( m_sessionMode ) )
        {
            CloseHandle(m_hEvent_SYNCOP_DEC_REQUEST );
            CloseHandle(m_hEvent_SYNCOP_DEC_RESPONSE );
            CloseHandle(m_hEvent_SYNCOP_DEC_RESPONSE_FINAL );
        }

        if( IsVPPEnable() )
        {
            CloseHandle( m_hEvent_VPP_REQUEST );
            CloseHandle( m_hEvent_VPP_RESPONSE );
            CloseHandle( m_hEvent_VPP_RESPONSE_FINAL );

            if( IsVppSyncOperationRequired( m_sessionMode ) )
            {
                CloseHandle(m_hEvent_SYNCOP_VPP_REQUEST );
                CloseHandle(m_hEvent_SYNCOP_VPP_RESPONSE );
                CloseHandle(m_hEvent_SYNCOP_VPP_RESPONSE_FINAL );
            }
        }

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

    HANDLE hThread[THREADS_COUNT] = {0};
    int threadIndx;
    for (threadIndx = 0; threadIndx < threadCount; threadIndx++)
    {
        threadParamList[threadIndx].pClass  = this;
        threadParamList[threadIndx].pMethod = routineList[ threadIndx ];
        threadParamList[threadIndx].pParam  = NULL;

        hThread[threadIndx] = (HANDLE)_beginthreadex(0,
                                                     0,
                                                     threadFuncList[threadIndx],
                                                     (void *)&threadParamList[threadIndx],
                                                      0,
                                                      0);
    }

    // start processing
    BOOL bSts = SetEvent(m_hEvent_BSR_RESPONSE);// GO-GO-GO

    if( 0 == bSts)
    {
        DWORD errSts =  GetLastError();
        printf("\nERR: %x\n", errSts);
    }
    
    for (threadIndx = 0; threadIndx < threadCount; threadIndx++)
    {
        WaitForSingleObject(hThread[threadIndx], INFINITE);
    }    

    return sts;

} // mfxStatus TranscodeModelAdvanced::Run()


/* ******************************************************************** */
/*     implementation of Test Usage Model #1 thread functions           */
/* ******************************************************************** */

mfxU32 TranscodeModelAdvanced::BSReaderRoutine(void *pParam)
{
    pParam;// warning skip

    mfxStatus mfxSts;
    HANDLE hEventWaitList[2];

    hEventWaitList[0] = m_hEvent_DEC_REQUEST;
    hEventWaitList[1] = m_hEvent_GLOBAL_ERR;

    printf("\nBSReaderRoutine: \tstart");

    for(;;)
    {
        DWORD dwSts = WaitForMultipleObjects(2, hEventWaitList, false, TUM_WAIT_INTERVAL);

        switch( dwSts )
        {
        case WAIT_OBJECT_0: // m_hEvent_DEC_REQUEST
            {
                // main operation here
                mfxSts = ReadOneFrame();//critical section???
                if(MFX_ERR_NONE == mfxSts)
                {
                    SetEvent( m_hEvent_BSR_RESPONSE );
                }
                else if ( MFX_ERR_MORE_DATA == mfxSts )
                {
                    SetEvent( m_hEvent_BSR_RESPONSE_FINAL );

                    printf("\n\nBSReaderRoutine: \texit with OK");//EXIT from thread
                    return TUM_OK_STS;//OK
                }
                else
                {                    
                    SetEvent( m_hEvent_GLOBAL_ERR );
                    RETURN_ON_ERROR( _T("\nBSReaderRoutine: exit with FAIL_1"), TUM_ERR_STS);
                }

                break;
            }
        case WAIT_OBJECT_0 + 1: // m_hEvent_GLOBAL_ERR
            {
                RETURN_ON_ERROR( _T("\nBSReaderRoutine: exit with GLOBAL_ERR"), TUM_ERR_STS);
            }
        default:
            {
                SetEvent( m_hEvent_GLOBAL_ERR );
                RETURN_ON_ERROR( _T("\nBSReaderRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
            }
        
        }
    }

    //RETURN_ON_ERROR( _T("\nBSREADER: exit with UKNOWN ERROR\n"), TUM_ERR_STS);

} // mfxU32 __stdcall TranscodeModelAdvanced::BSReaderRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::DecodeRoutine(void *pParam)
{
    pParam; // warninh disable

    HANDLE hEventWaitList[4];

    // wait list
    hEventWaitList[0] = m_hEvent_BSR_RESPONSE;
    hEventWaitList[1] = m_hEvent_BSR_RESPONSE_FINAL;
    if( IsDecodeSyncOperationRequired( m_sessionMode ) )
    {
        hEventWaitList[2] = m_hEvent_SYNCOP_DEC_REQUEST; // changed depends on session model usage
    }
    else
    {
        if( IsVPPEnable() )
        {
            hEventWaitList[2] = m_hEvent_VPP_REQUEST; // changed depends on session model usage
        }
        else
        {
            hEventWaitList[2] = m_hEvent_ENC_REQUEST; // changed depends on session model usage
        }
    }
    hEventWaitList[3] = m_hEvent_GLOBAL_ERR;

    // send list
    HANDLE hEventRequest       = m_hEvent_DEC_REQUEST;
    HANDLE hEventResponse      = m_hEvent_DEC_RESPONSE;
    HANDLE hEventResponseFinal = m_hEvent_DEC_RESPONSE_FINAL;

    bool bRequestEnable = true;
    TUMSurfacesManager* pSurfMngr;
    Surface1ExList*     pConnectPool;

    if( IsVPPEnable() )
    {
        pSurfMngr = &m_dec2vppSurfManager;
        pConnectPool = &m_dec2vppSurfExPool;
    }
    else
    {
        pSurfMngr = &m_dec2encSurfManager;
        pConnectPool = &m_dec2encSurfExPool;
    }

    printf("\nDecodeRoutine: \t\tstart");

    for(;;)
    {
        DWORD dwSts = WaitForMultipleObjects( 4, hEventWaitList, false, TUM_WAIT_INTERVAL); 

        if( WAIT_OBJECT_0 + 0 == dwSts ) // m_hEvent_BSR_RESPONSE
        {
            mfxFrameSurfaceEx decSurfaceEx = {0};
            mfxBitstream *pBS = GetSrcPtr(); //critical section???

            mfxStatus mfxSts = DecodeOneFrame(pBS, pSurfMngr, &decSurfaceEx);

            //-----------------------------------------------------------------
            if( MFX_ERR_MORE_DATA == mfxSts )
            {
                if( bRequestEnable )
                {
                    SetEvent( hEventRequest );                    
                }
                else
                {          
                    // to get buffered VPP or ENC frames
                    decSurfaceEx.pSurface = NULL;
                    decSurfaceEx.syncp = 0;

                    pConnectPool->push_back(decSurfaceEx);// critical section???

                    SetEvent( hEventResponseFinal );

                    printf("\nDecodeRoutine: \t\texit with OK");
                    return TUM_OK_STS; // EXIT from thread
                }            
            }
            else if (MFX_ERR_NONE == mfxSts) 
            {
                pConnectPool->push_back(decSurfaceEx);// critical section???
                SetEvent( hEventResponse );
            }
            else
            {
                SetEvent( m_hEvent_GLOBAL_ERR );
                RETURN_ON_ERROR( _T("\nDecodeRoutine: exit with FAIL_1"), TUM_ERR_STS);
            }
            //-----------------------------------------------------------------
        }
        else if( WAIT_OBJECT_0 + 1 == dwSts ) // m_hEvent_BSR_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent( hEventWaitList[0] ); // force external response
        }
        else if( WAIT_OBJECT_0 + 2 == dwSts ) // m_hEvent_VPP_REQUEST
        {
            SetEvent( hEventWaitList[0] ); // force external response
        }
        else if( WAIT_OBJECT_0 + 3 == dwSts ) // m_hEvent_GLOBAL_ERR
        {
            RETURN_ON_ERROR( _T("\nDecodeRoutine: exit with GLOBAL_ERR\n"), TUM_ERR_STS);
        }
        else
        {
            SetEvent( m_hEvent_GLOBAL_ERR );
            RETURN_ON_ERROR( _T("\nDecodeRoutine: exit with UNKNOWN ERR\n"), TUM_ERR_STS);
        }
    }

    //RETURN_ON_ERROR( _T("\nDecodeRoutine: exit with FAIL_3\n"), TUM_ERR_STS);

} // mfxU32 __stdcall TranscodeModelAdvanced::DecodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::VPPRoutine(void *pParam)
{
    pParam;// warning disable

    HANDLE hEventWaitList[4];
    
    // wait list
    if( IsDecodeSyncOperationRequired( m_sessionMode ) )
    {
        hEventWaitList[0] = m_hEvent_SYNCOP_DEC_RESPONSE;       // depends on session model
        hEventWaitList[1] = m_hEvent_SYNCOP_DEC_RESPONSE_FINAL; // depends on session model
    }
    else
    {
        hEventWaitList[0] = m_hEvent_DEC_RESPONSE;       
        hEventWaitList[1] = m_hEvent_DEC_RESPONSE_FINAL; 
    }

    if( IsVppSyncOperationRequired( m_sessionMode ) )
    {
        hEventWaitList[2] = m_hEvent_SYNCOP_VPP_REQUEST;        // depends on session model
    }
    else
    {
        hEventWaitList[2] = m_hEvent_ENC_REQUEST;        // depends on session model
    }
    hEventWaitList[3] = m_hEvent_GLOBAL_ERR;

    // send list
    HANDLE hEventRequest       = m_hEvent_VPP_REQUEST;
    HANDLE hEventResponse      = m_hEvent_VPP_RESPONSE;
    HANDLE hEventResponseFinal = m_hEvent_VPP_RESPONSE_FINAL;

    bool bRequestEnable = true;
    mfxFrameSurfaceEx zeroSurfaceEx = {NULL, 0};

    printf("\nVPPRoutine: \t\tstart");

    for(;;)
    {
        DWORD dwSts = WaitForMultipleObjects( 4, hEventWaitList, false, TUM_WAIT_INTERVAL); 

        if( WAIT_OBJECT_0 + 0 == dwSts ) // m_hEvent_DEC_RESPONSE
        {
            mfxFrameSurfaceEx outputSurfaceEx = {0};
            mfxFrameSurfaceEx inputSurfaceEx = ( m_dec2vppSurfExPool.size() ) ? m_dec2vppSurfExPool.front() : zeroSurfaceEx;//critical section            

            mfxStatus mfxSts = VPPOneFrame(inputSurfaceEx.pSurface, &m_vpp2encSurfManager, &outputSurfaceEx);   

            // after VPPOneFrame() we should erase first element. real data is kept by mfx_session
            if( m_dec2vppSurfExPool.size() > 0 )
            {
                m_dec2vppSurfExPool.pop_front();
            }

            //-----------------------------------------------------------------
            if( MFX_ERR_MORE_DATA == mfxSts )
            {
                if( bRequestEnable )
                {
                    SetEvent( hEventRequest );
                }
                else
                {          
                    // to get buffered VPP or ENC frames
                    outputSurfaceEx.pSurface = NULL;
                    outputSurfaceEx.syncp = 0;

                    m_vpp2encSurfExPool.push_back(outputSurfaceEx);// critical section???

                    SetEvent( hEventResponseFinal );

                    printf("\nVPPRoutine: \t\texit with OK");
                    return TUM_OK_STS; // EXIT from thread
                }            
            }
            else if (MFX_ERR_NONE == mfxSts) 
            {
                m_vpp2encSurfExPool.push_back(outputSurfaceEx);// critical section???
                SetEvent( hEventResponse );
            }
            else
            {   
                SetEvent( m_hEvent_GLOBAL_ERR );
                RETURN_ON_ERROR( _T("\nVPPRoutine: exit with FAIL_1"), TUM_ERR_STS);                
            }
            //-----------------------------------------------------------------
        }
        else if( WAIT_OBJECT_0 + 1 == dwSts ) // m_hEvent_DEC_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent( hEventWaitList[0] );
        }
        else if( WAIT_OBJECT_0 + 2 == dwSts ) // m_hEvent_ENC_REQUEST
        {
            if( m_dec2vppSurfExPool.size() > 0 ) // we have frames in input frames pool
            {
                SetEvent( hEventWaitList[0] );
            }
            else if ( bRequestEnable ) // we require frames from previous (VPP) component
            {
                SetEvent( hEventRequest );
            }
            else // we start "last frame" processing
            {
                SetEvent( hEventWaitList[0] );
            }
        }
        else if( WAIT_OBJECT_0 + 3 == dwSts ) // m_hEvent_GLOBAL_ERR
        {
            RETURN_ON_ERROR( _T("\nVPPRoutine: exit with GLOBAL ERR"), TUM_ERR_STS);
        }
        else if( WAIT_TIMEOUT == dwSts ) // timeout
        {
            SetEvent( m_hEvent_GLOBAL_ERR );
            RETURN_ON_ERROR( _T("\nVPPRoutine: exit with FAIL TIMEOUT"), TUM_ERR_STS);
        }
        else
        {   
            SetEvent( m_hEvent_GLOBAL_ERR );
            RETURN_ON_ERROR( _T("\nVPPRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }
    }
    
    //RETURN_ON_ERROR( _T("\nVPPRoutine: exit with UNKNOWN_ERR\n"), TUM_ERR_STS);

} // mfxU32 __stdcall TranscodeModelAdvanced::VPPRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::EncodeRoutine(void *pParam)
{
    pParam; // disable

    HANDLE hEventWaitList[4];

    // wait list
    if( IsVPPEnable() )
    {
        if( IsVppSyncOperationRequired( m_sessionMode ) )
        {
            hEventWaitList[0] = m_hEvent_SYNCOP_VPP_RESPONSE;
            hEventWaitList[1] = m_hEvent_SYNCOP_VPP_RESPONSE_FINAL; 
        }
        else
        {
            hEventWaitList[0] = m_hEvent_VPP_RESPONSE;
            hEventWaitList[1] = m_hEvent_VPP_RESPONSE_FINAL;
        }
    }
    else
    {
        hEventWaitList[0] = m_hEvent_DEC_RESPONSE;
        hEventWaitList[1] = m_hEvent_DEC_RESPONSE_FINAL;
    }

    hEventWaitList[2] = m_hEvent_BSWRT_RESPONSE;    
    hEventWaitList[3] = m_hEvent_GLOBAL_ERR;

    // send list
    HANDLE hEventRequest       = m_hEvent_ENC_REQUEST;
    HANDLE hEventResponse      = m_hEvent_ENC_RESPONSE;
    HANDLE hEventResponseFinal = m_hEvent_ENC_RESPONSE_FINAL;

    bool bRequestEnable = true;
    mfxFrameSurfaceEx zeroSurfaceEx = {NULL, 0};

    Surface1ExList*     pConnectPool;

    if( IsVPPEnable() )
    {
        pConnectPool = &m_vpp2encSurfExPool;
    }
    else
    {
        pConnectPool = &m_dec2encSurfExPool;
    }
    printf("\nEncodeRoutine: \t\tstart");

    for(;;)
    {
        DWORD dwSts = WaitForMultipleObjects( 4, hEventWaitList, false, TUM_WAIT_INTERVAL); 

        if( WAIT_OBJECT_0 + 0 == dwSts ) // m_hEvent_VPP_RESPONSE
        {
            mfxFrameSurfaceEx inputSurfaceEx = (pConnectPool->size() > 0 ) ? pConnectPool->front() : zeroSurfaceEx;//critical section???

            mfxBitstreamEx* pBSEx = NULL;
            pBSEx = m_pOutBSManager->GetNext();
            pBSEx->syncp = 0;

            mfxStatus mfxSts = EncodeOneFrame( inputSurfaceEx.pSurface, pBSEx );

            // after EncodeOneFrame() we should erase first element. real data is kept by mfx_session
            if(pConnectPool->size() > 0 )
            { 
                pConnectPool->pop_front();
            }

            //-----------------------------------------------------------------
            if( MFX_ERR_MORE_DATA == mfxSts )
            {
                // the task in not in Encode queue
                m_pOutBSManager->Release( pBSEx );

                if( bRequestEnable )
                {
                    SetEvent( hEventRequest );
                }
                else
                {                     
                    SetEvent( hEventResponseFinal );

                    printf("\nEncodeRoutine: \t\texit with OK");
                    return TUM_OK_STS; // EXIT from thread
                }            
            }
            else if (MFX_ERR_NONE == mfxSts) 
            {
                m_outBSPool.push_back( pBSEx );// critical section???
                SetEvent( hEventResponse );
            }
            else
            {   
                SetEvent( m_hEvent_GLOBAL_ERR );
                RETURN_ON_ERROR( _T("\nEncodeRoutine: exit with FAIL_1"), TUM_ERR_STS);
            }
            //-----------------------------------------------------------------
        }
        else if( WAIT_OBJECT_0 + 1 == dwSts ) // m_hEvent_VPP_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent( hEventWaitList[0] );
        }
        else if( WAIT_OBJECT_0 + 2 == dwSts ) // m_hEvent_BSWRT_REQUEST
        {
            if( pConnectPool->size() > 0 ) // we have frames in input frames pool
            {
                SetEvent( hEventWaitList[0] );
            }
            else if ( bRequestEnable ) // we require frames from previous (VPP) component
            {
                SetEvent( hEventRequest );
            }
            else // we start "last frame" processing
            {
                SetEvent( hEventWaitList[0] );
            }
        }
        else if( WAIT_OBJECT_0 + 3 == dwSts ) // // m_hEvent_GLOBAL_ERR
        {
            RETURN_ON_ERROR( _T("\nEncodeRoutine: exit with GLOBAL ERR"), TUM_ERR_STS);
        }
        else
        {      
            SetEvent( m_hEvent_GLOBAL_ERR );
            RETURN_ON_ERROR( _T("\nEncodeRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }
    }
    
    //RETURN_ON_ERROR( _T("\nEncodeRoutine: exit with UNKNOWN_ERR\n"), TUM_ERR_STS);

} // mfxU32 __stdcall TranscodeModelAdvanced::EncodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::SyncOperationDecodeRoutine(void *pParam)
{
    pParam; // disable

    HANDLE hEventWaitList[4];

    // wait list
    hEventWaitList[0] = m_hEvent_DEC_RESPONSE;
    hEventWaitList[1] = m_hEvent_DEC_RESPONSE_FINAL;
    if( IsVPPEnable() )
    {
        hEventWaitList[2] = m_hEvent_VPP_REQUEST;
    }
    else
    {
        hEventWaitList[2] = m_hEvent_ENC_REQUEST;
    }
    hEventWaitList[3] = m_hEvent_GLOBAL_ERR;

    // send list
    HANDLE hEventRequest       = m_hEvent_SYNCOP_DEC_REQUEST;
    HANDLE hEventResponse      = m_hEvent_SYNCOP_DEC_RESPONSE;
    HANDLE hEventResponseFinal = m_hEvent_SYNCOP_DEC_RESPONSE_FINAL;

    bool bRequestEnable = true;

    mfxFrameSurfaceEx zeroSurfaceEx = {NULL, 0};

    Surface1ExList*    pConnectPool;

    if( IsVPPEnable() )
    {
        pConnectPool = &m_dec2vppSurfExPool;
    }
    else
    {
        pConnectPool = &m_dec2encSurfExPool;
    }

    printf("\nSyncOpDECRoutine:\tstart");

    for(;;)
    {
        DWORD dwSts = WaitForMultipleObjects( 4, hEventWaitList, false, TUM_WAIT_INTERVAL );

        if( WAIT_OBJECT_0 + 0 == dwSts ) // m_hEvent_DEC_RESPONSE
        {
            //if( (m_dec2vppSurfExPool.size() != m_asyncDepth) && (bRequestEnable) )
            //{
            //    SetEvent( hEventRequest );
            //}
            ////------------------------------------------------------------------
            //else // normal processing
            {
                mfxFrameSurfaceEx surfaceEx = ( pConnectPool->size() ) ? pConnectPool->front() : zeroSurfaceEx;//critical section            

                if( 0 == surfaceEx.syncp )
                {
                    if( !bRequestEnable )
                    {
                        SetEvent( hEventResponseFinal );

                        printf("\nSyncOpDECRoutine:\texit with OK");
                        return TUM_OK_STS; // EXIT from thread
                    }
                    else
                    {
                        SetEvent( m_hEvent_GLOBAL_ERR );
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
                        SetEvent( hEventResponse );
                    }
                    else
                    {
                        SetEvent( m_hEvent_GLOBAL_ERR );
                        RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine: FAIL"), TUM_ERR_STS);
                    }
                }

            }
        }
        else if( WAIT_OBJECT_0 + 1 == dwSts ) // m_hEvent_DEC_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent( hEventWaitList[0] );
        }
        else if( WAIT_OBJECT_0 + 2 == dwSts ) // m_hEvent_VPP_REQUEST
        {
            if ( bRequestEnable ) // we require frames from previous component
            {
                SetEvent( hEventRequest );
            }
            else // we start "last frame" processing
            {
                SetEvent( hEventWaitList[0] );
            }
        }
        else
        {
            SetEvent( m_hEvent_GLOBAL_ERR );
            RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }        
    }

} // mfxU32 TranscodeModelAdvanced::SyncOperationDecodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::SyncOperationVPPRoutine(void *pParam)
{
    pParam; // disable

    HANDLE hEventWaitList[4];

    // wait list
    hEventWaitList[0] = m_hEvent_VPP_RESPONSE;
    hEventWaitList[1] = m_hEvent_VPP_RESPONSE_FINAL;
    hEventWaitList[2] = m_hEvent_ENC_REQUEST;
    hEventWaitList[3] = m_hEvent_GLOBAL_ERR;

    // send list
    HANDLE hEventRequest       = m_hEvent_SYNCOP_VPP_REQUEST;
    HANDLE hEventResponse      = m_hEvent_SYNCOP_VPP_RESPONSE;
    HANDLE hEventResponseFinal = m_hEvent_SYNCOP_VPP_RESPONSE_FINAL;

    bool bRequestEnable = true;

    mfxFrameSurfaceEx zeroSurfaceEx = {NULL, 0};

    printf("\nSyncOpVPPRoutine: \tstart");

    for(;;)
    {
        DWORD dwSts = WaitForMultipleObjects( 4, hEventWaitList, false, TUM_WAIT_INTERVAL );

        if( WAIT_OBJECT_0 + 0 == dwSts ) // m_hEvent_VPP_RESPONSE
        {
            //if( (m_vpp2encSurfExPool.size() != m_asyncDepth) && (bRequestEnable) )
            //{
            //    SetEvent( hEventRequest );
            //}
            ////------------------------------------------------------------------
            //else // normal processing
            {
                mfxFrameSurfaceEx surfaceEx = ( m_vpp2encSurfExPool.size() ) ? m_vpp2encSurfExPool.front() : zeroSurfaceEx;//critical section            

                if( 0 == surfaceEx.syncp )
                {
                    if( !bRequestEnable )
                    {
                        SetEvent( hEventResponseFinal );

                        printf("\nSyncOperationDECRoutine:exit with OK");
                        return TUM_OK_STS; // EXIT from thread
                    }
                    else
                    {
                        SetEvent( m_hEvent_GLOBAL_ERR );
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
                        SetEvent( hEventResponse );
                    }
                    else
                    {
                        SetEvent( m_hEvent_GLOBAL_ERR );
                        RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine: FAIL"), TUM_ERR_STS);
                    }
                }

            }
        }
        else if( WAIT_OBJECT_0 + 1 == dwSts ) // m_hEvent_DEC_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent( hEventWaitList[0] );
        }
        else if( WAIT_OBJECT_0 + 2 == dwSts ) // m_hEvent_VPP_REQUEST
        {
            if ( bRequestEnable ) // we require frames from previous component
            {
                SetEvent( hEventRequest );
            }
            else // we start "last frame" processing
            {
                SetEvent( hEventWaitList[0] );
            }
        }
        else
        {
            SetEvent( m_hEvent_GLOBAL_ERR );
            RETURN_ON_ERROR( _T("\nSyncOperationDECRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }        
    }

} // mfxU32 TranscodeModelAdvanced::SyncOperationDecodeRoutine(void *pParam)


mfxU32 TranscodeModelAdvanced::BSWriterRoutine(void *pParam)
{
    pParam; // warning disable

    HANDLE hEventWaitList[3];

    hEventWaitList[0] = m_hEvent_ENC_RESPONSE;
    hEventWaitList[1] = m_hEvent_ENC_RESPONSE_FINAL;
    hEventWaitList[2] = m_hEvent_GLOBAL_ERR;

    bool bRequestEnable = true;

    printf("\nBSWriterRoutine: \tstart");

    for(;;)
    {
        DWORD dwSts = WaitForMultipleObjects( 3, hEventWaitList, false, TUM_WAIT_INTERVAL ); 

        if( WAIT_OBJECT_0 + 0 == dwSts ) // m_hEvent_ENC_RESPONSE
        {
            if( 0 == m_outBSPool.size() )
            {
                printf("\nBSWriterRoutine: \texit with OK");
                return TUM_OK_STS; // EXIT from thread
            }
            if( (m_outBSPool.size() != m_asyncDepth) && (bRequestEnable) )
            {
                //printf("\n BSWRT: async_pool not ready yet \n");
                // continue transcode processing wo synchronization
                SetEvent( m_hEvent_BSWRT_RESPONSE ); // bitstream response == request 
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

                    // we should realise captured element/resource. 
                    m_outBSPool.pop_front(); // critical section       
                    m_pOutBSManager->Release( pBSEx );

                    if( bRequestEnable )
                    {
                        SetEvent( m_hEvent_BSWRT_RESPONSE );
                    }
                    else
                    {
                        SetEvent( hEventWaitList[0] );                        
                    }
                }
                else
                { 
                    SetEvent( m_hEvent_GLOBAL_ERR );
                    RETURN_ON_ERROR( _T("\nSynchronizeEncode: FAIL"), TUM_ERR_STS);
                }
            }
            //-----------------------------------------------------------------
        }
        else if( WAIT_OBJECT_0 + 1 == dwSts ) // m_hEvent_ENC_RESPONSE_FINAL
        {
            bRequestEnable = false;
            SetEvent( hEventWaitList[0] );
        }
        else if( WAIT_OBJECT_0 + 2 == dwSts ) // m_hEvent_GLOBAL_ERR
        {
            RETURN_ON_ERROR( _T("\nBSWriterRoutine: exit with UNKNOWN ERR"), TUM_ERR_STS);
        }
        else
        {
            SetEvent( m_hEvent_GLOBAL_ERR );
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
