//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//

#include "transcode_model_reference.h"
#include "vm/time_defs.h"

/* ******************************************************************** */
/*             prototypes of service functions                          */
/* ******************************************************************** */

mfxStatus ConfigDecodeParam( AppParam param, mfxVideoParam bsHeaderParam, mfxVideoParam& decodeVideoParam);
mfxStatus ConfigEncodeParam( AppParam param, mfxVideoParam vppVideoParam, mfxVideoParam& encodeVideoParam);
mfxStatus ConfigVppParam( AppParam param, mfxVideoParam decodeVideoParam, mfxVideoParam& vppVideoParam);
bool IsVppRequired( mfxVideoParam vppVideoParam );

mfxStatus CreateVppExtension( AppParam param, mfxVideoParam& vppVideoParam );
mfxStatus DestroyVppExtension( mfxVideoParam& vppVideoParam );

mfxStatus InitMFXSession( MFXVideoSession** ppSession, mfxIMPL impl )
{   
    MSDK_CHECK_POINTER(ppSession, MFX_ERR_NULL_PTR);

    mfxVersion version = {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};

    *ppSession = new MFXVideoSession;

    mfxStatus sts = (*ppSession)->Init(impl, &version);

    return sts;

} // mfxStatus InitMFXSession( MFXVideoSession** ppSession )


mfxStatus CloseMFXSession( MFXVideoSession* pSession )
{   
    MSDK_CHECK_POINTER(pSession, MFX_ERR_NULL_PTR);

    mfxStatus sts = pSession->Close();
    delete pSession;

    return sts;

} // mfxStatus CloseMFXSession( MFXVideoSession* pSession )


template <typename MFXComponent> 
mfxStatus GetFrameAllocRequest(MFXComponent* pComp, mfxVideoParam* pParam, mfxFrameAllocRequest *pRequest)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts =  pComp->Query(pParam, pParam);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    
    sts =  pComp->QueryIOSurf(pParam, pRequest);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;

} // mfxStatus TUMQueryIOSurf(...)

/* ******************************************************************** */
/*               implementation of TranscodeModelReference              */
/* ******************************************************************** */

TranscodeModelReference::TranscodeModelReference(AppParam& param)
    : m_bsReader( param.pSrcFileName )
    , m_bsWriter( param.pDstFileName )
    , m_allocator()
    , m_bInited(false)
    , m_bVPPEnable(false)
    , m_framesCount(0)
    , m_sessionMode(DECVPPENC_SESSION)
// other parameters inited in function
{

} // TranscodeModelReference::TranscodeModelReference(AppParam& param)


TranscodeModelReference::~TranscodeModelReference(void)
{
    mfxStatus mfxSts = Close();
    
    printf("\ntranscoded frames: %i\n", m_framesCount);
    
    mfxSts;

} // TranscodeModelReference::~TranscodeModelReference()


mfxStatus TranscodeModelReference::Init( AppParam& params )
{
    mfxStatus sts;
   
    m_sessionMode = params.sessionMode;
    sts = InitMFXSessions( m_sessionMode, params.impLib );
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    
    // component creation
    m_pmfxDEC = new MFXVideoDECODE( *m_pSessionDecode );    
    m_pmfxVPP = new MFXVideoVPP( *m_pSessionVPP );
    m_pmfxENC = new MFXVideoENCODE( *m_pSessionEncode );

    // reset parameters
    MSDK_ZERO_MEMORY(m_decodeVideoParam);
    MSDK_ZERO_MEMORY(m_vppVideoParam);
    MSDK_ZERO_MEMORY(m_encodeVideoParam);

    sts = ConfigMFXComponents( params );
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = CreateAllocator( params.IOPattern );
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // on Linux SetHandle call is mandatory before any components' calls
#if defined (LIBVA_SUPPORT)
    sts = m_pSessionDecode->SetHandle(MFX_HANDLE_VA_DISPLAY, static_cast<mfxHDL>(m_allocator.va_display));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pSessionVPP->SetHandle(MFX_HANDLE_VA_DISPLAY, static_cast<mfxHDL>(m_allocator.va_display));
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pSessionEncode->SetHandle(MFX_HANDLE_VA_DISPLAY, static_cast<mfxHDL>(m_allocator.va_display));
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#endif

    // QueryIOSurf()
    //{
        mfxFrameAllocRequest decodeFrameRequest = {0};
        mfxFrameAllocRequest vppFrameRequest[2] = {0};
        mfxFrameAllocRequest encodeFrameRequest = {0};

        sts = GetFrameAllocRequest(m_pmfxDEC, &m_decodeVideoParam, &decodeFrameRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = GetFrameAllocRequest(m_pmfxENC, &m_encodeVideoParam, &encodeFrameRequest); 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        if( IsVPPEnable() )
        {
            sts = GetFrameAllocRequest(m_pmfxVPP, &m_vppVideoParam,    &(vppFrameRequest[0])); 
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

    //} // QueryIOSurf 


    // calculation of component-to-component connection frames count
    if( IsVPPEnable() )
    {
        mfxFrameAllocRequest dec2vppFrameRequest = decodeFrameRequest;
        dec2vppFrameRequest.NumFrameMin = decodeFrameRequest.NumFrameMin + vppFrameRequest[0].NumFrameMin - 1;
        dec2vppFrameRequest.NumFrameSuggested = decodeFrameRequest.NumFrameSuggested + vppFrameRequest[0].NumFrameSuggested - 1;

        mfxFrameAllocRequest vpp2encFrameRequest = encodeFrameRequest;
        vpp2encFrameRequest.NumFrameMin = encodeFrameRequest.NumFrameMin + vppFrameRequest[1].NumFrameMin - 1;        
        vpp2encFrameRequest.NumFrameSuggested = encodeFrameRequest.NumFrameSuggested + vppFrameRequest[1].NumFrameSuggested - 1;

        // to take into account m_asyncDepth
        if( params.asyncDepth > 1 ) // we have async mode of transcode
        {
            dec2vppFrameRequest.NumFrameMin       += (params.asyncDepth-1);
            dec2vppFrameRequest.NumFrameSuggested += (params.asyncDepth-1);

            vpp2encFrameRequest.NumFrameMin       += (params.asyncDepth-1);
            vpp2encFrameRequest.NumFrameSuggested += (params.asyncDepth-1);
        }


    sts = CreateAllocator( params.IOPattern );
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


        // surfaces manager ()
        {        
            sts = m_dec2vppSurfManager.Init( dec2vppFrameRequest, m_allocator.pMfxAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    
            sts = m_vpp2encSurfManager.Init( vpp2encFrameRequest, m_allocator.pMfxAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }
    else
    {
        mfxFrameAllocRequest dec2encFrameRequest = decodeFrameRequest;
        dec2encFrameRequest.NumFrameMin = decodeFrameRequest.NumFrameMin + encodeFrameRequest.NumFrameMin - 1;
        dec2encFrameRequest.NumFrameSuggested = decodeFrameRequest.NumFrameSuggested + encodeFrameRequest.NumFrameSuggested - 1;

        // to take into account m_asyncDepth
        if( params.asyncDepth > 1 ) // we have async mode of transcode
        {
            dec2encFrameRequest.NumFrameMin       += (params.asyncDepth-1);
            dec2encFrameRequest.NumFrameSuggested += (params.asyncDepth-1);        
        }

        // surfaces manager ()
        {
            sts = m_dec2encSurfManager.Init( dec2encFrameRequest, m_allocator.pMfxAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    // Allocator for MediaSDK session in case of D3D surfaces using
#if !defined(LIBVA_SUPPORT)
    if( params.IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY) ) 
    {
#endif
        sts = SetAllocatorMFXSessions( params.IOPattern );
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#if !defined(LIBVA_SUPPORT)
    }
#endif

    //Join Session
    if( IsJoinSessionEnable() )
    {
        sts = JoinMFXSessions( m_sessionMode );
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // Init MFX components
    sts = InitMFXComponents( );
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // number of transcoded frames
    m_framesCount = 0;

    // init successfull
    m_bInited = true;

    return MFX_ERR_NONE;

} // mfxStatus TranscodeModelReference::Init( AppParam& params );


mfxStatus  TranscodeModelReference::Close(void)
{
    if( m_bInited )
    {
        mfxStatus sts;

        // component
        sts = m_pmfxDEC->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);    

        if( IsVPPEnable() )
        {
            sts = m_pmfxVPP->Close();
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);    
        }

        sts = m_pmfxENC->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);

        MSDK_SAFE_DELETE(m_pmfxDEC);
        MSDK_SAFE_DELETE(m_pmfxVPP);
        MSDK_SAFE_DELETE(m_pmfxENC);

        if( IsJoinSessionEnable() )
        {
            DisjoinMFXSessions( m_sessionMode );
        }
                     
        CloseMFXSessions( m_sessionMode );

        if( IsVPPEnable() )
        {
            m_dec2vppSurfManager.Close();
            m_vpp2encSurfManager.Close();
        }
        else
        {
            m_dec2encSurfManager.Close();
        }

        DestroyAllocator();

        // release of VPP extension buffers        
        DestroyVppExtension( m_vppVideoParam );

        // reset
        m_bInited = false;
    }

    return MFX_ERR_NONE;

} // mfxStatus TranscodeModelReference::Close( void )


mfxStatus TranscodeModelReference::ConfigMFXComponents( AppParam& param )
{
    mfxStatus sts;

    // read bitstream header param
    mfxVideoParam bsHeaderParam;
    MSDK_ZERO_MEMORY(bsHeaderParam);

    bsHeaderParam.mfx.CodecId = param.srcVideoFormat;    
    sts = ReadBitstreamHeader( bsHeaderParam );
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // configuration of parameters
    sts = ConfigDecodeParam( param, bsHeaderParam, m_decodeVideoParam);//[in, in, out]
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = ConfigVppParam( param, m_decodeVideoParam, m_vppVideoParam);//[in, in, out]
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    m_bVPPEnable = IsVppRequired( m_vppVideoParam );
    sts = ConfigEncodeParam( param, m_vppVideoParam, m_encodeVideoParam);//[in, in, out] 
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);    
        

    return sts;
} // mfxStatus TranscodeModelReference::ConfigMFXComponents( AppParam& param )


bool IsVppRequired( mfxVideoParam vppVideoParam )
{
    // (1) VPP_RESIZE/CROPPING is supported now only. 
    // (2) FRC/DI/DN/CSCDTL/PROCAMP will be added late
    mfxFrameInfo* pIn  = &(vppVideoParam.vpp.In);
    mfxFrameInfo* pOut = &(vppVideoParam.vpp.Out);

    bool bResize = ((pIn->Height != pOut->Height) || (pIn->Width != pOut->Width)) ? true : false;
    bool bCrop   = ( (pIn->CropW != pOut->CropW) || 
                     (pIn->CropH != pOut->CropH) || 
                     (pIn->CropX != pOut->CropX) ||
                     (pIn->CropY != pOut->CropY)) ? true : false;

    return (bResize || bCrop);

} // bool IsVppRequired( mfxVideoParam vppVideoParam )


mfxStatus TranscodeModelReference::InitMFXComponents( void )
{
    mfxStatus sts;

    // component initialization
    sts = m_pmfxDEC->Init(&m_decodeVideoParam);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if( IsVPPEnable() )
    {
        sts = m_pmfxVPP->Init(&m_vppVideoParam);
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    sts = m_pmfxENC->Init(&m_encodeVideoParam);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;

} // mfxStatus TranscodeModelReference::InitMFXComponents( void )
    

mfxStatus TranscodeModelReference::InitMFXSessions( SessionMode mode, std::map<msdk_char*, mfxIMPL> impl)
{
    mfxStatus sts = MFX_ERR_UNDEFINED_BEHAVIOR;            
 
    switch( mode )
    {
        case DECVPPENC_SESSION:
        {
            mfxIMPL implDec = (impl.find(MSDK_STRING("dec")) != impl.end()) ? impl[MSDK_STRING("dec")] : impl[MSDK_STRING("general")];
            mfxIMPL implVpp = (impl.find(MSDK_STRING("vpp")) != impl.end()) ? impl[MSDK_STRING("vpp")] : impl[MSDK_STRING("general")];
            mfxIMPL implEnc = (impl.find(MSDK_STRING("enc")) != impl.end()) ? impl[MSDK_STRING("enc")] : impl[MSDK_STRING("general")];

            mfxIMPL implGeneral = impl[MSDK_STRING("general")];
            if( (implDec == implVpp) && (implVpp == implEnc) )
            {
                implGeneral = implVpp;
            }
            else
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            sts = InitMFXSession( &m_pSessionDecode, implGeneral );
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            m_pSessionVPP    = m_pSessionDecode;
            m_pSessionEncode = m_pSessionDecode;

            break;
        }

        case DEC_VPP_ENC_SESSION:
        {            
            mfxIMPL implDec = (impl.find(MSDK_STRING("dec")) != impl.end()) ? impl[MSDK_STRING("dec")] : impl[MSDK_STRING("general")]; 
            sts = InitMFXSession( &m_pSessionDecode, implDec);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            mfxIMPL implVpp = (impl.find(MSDK_STRING("vpp")) != impl.end()) ? impl[MSDK_STRING("vpp")] : impl[MSDK_STRING("general")]; 
            sts = InitMFXSession( &m_pSessionVPP, implVpp);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            mfxIMPL implEnc = (impl.find(MSDK_STRING("enc")) != impl.end()) ? impl[MSDK_STRING("enc")] : impl[MSDK_STRING("general")]; 
            sts = InitMFXSession( &m_pSessionEncode, implEnc);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);            

            break;
        }

        case DECVPP_ENC_SESSION:
        {
            sts = InitMFXSession( &m_pSessionDecode, impl[MSDK_STRING("general")]);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            m_pSessionVPP    = m_pSessionDecode;

            sts = InitMFXSession( &m_pSessionEncode, impl[MSDK_STRING("general")]);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);           

            break;
        }

        case DECENC_VPP_SESSION:
        {
            sts = InitMFXSession( &m_pSessionDecode, impl[MSDK_STRING("general")]);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            m_pSessionEncode    = m_pSessionDecode;

            sts = InitMFXSession( &m_pSessionVPP, impl[MSDK_STRING("general")]);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);           

            break;
        }

        case DEC_VPPENC_SESSION:
        {
            sts = InitMFXSession( &m_pSessionDecode, impl[MSDK_STRING("general")]);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);            

            sts = InitMFXSession( &m_pSessionVPP, impl[MSDK_STRING("general")]);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            m_pSessionEncode    = m_pSessionVPP;

            break;
        }

        // join sessions
        case DECVPP_ENC_JOIN_SESSION:
        {
            mfxIMPL implDec = (impl.find(MSDK_STRING("dec")) != impl.end()) ? impl[MSDK_STRING("dec")] : impl[MSDK_STRING("general")]; 

            sts = InitMFXSession( &m_pSessionDecode, implDec);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            m_pSessionVPP = m_pSessionDecode;

            mfxIMPL implEnc = (impl.find(MSDK_STRING("enc")) != impl.end()) ? impl[MSDK_STRING("enc")] : impl[MSDK_STRING("general")]; 
            sts = InitMFXSession( &m_pSessionEncode, implEnc);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            break;
        }

        case DEC_VPPENC_JOIN_SESSION:
        {
            mfxIMPL implDec = (impl.find(MSDK_STRING("dec")) != impl.end()) ? impl[MSDK_STRING("dec")] : impl[MSDK_STRING("general")]; 

            sts = InitMFXSession( &m_pSessionDecode, implDec);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);            

            mfxIMPL implEnc = (impl.find(MSDK_STRING("enc")) != impl.end()) ? impl[MSDK_STRING("enc")] : impl[MSDK_STRING("general")]; 
            sts = InitMFXSession( &m_pSessionEncode, implEnc);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            m_pSessionVPP = m_pSessionEncode;

            break;
        }

        case DEC_VPP_ENC_JOIN_SESSION:
        {
            mfxIMPL implDec = (impl.find(MSDK_STRING("dec")) != impl.end()) ? impl[MSDK_STRING("dec")] : impl[MSDK_STRING("general")]; 
            sts = InitMFXSession( &m_pSessionDecode, implDec);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            mfxIMPL implVpp = (impl.find(MSDK_STRING("vpp")) != impl.end()) ? impl[MSDK_STRING("vpp")] : impl[MSDK_STRING("general")]; 
            sts = InitMFXSession( &m_pSessionVPP, implVpp);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            mfxIMPL implEnc = (impl.find(MSDK_STRING("enc")) != impl.end()) ? impl[MSDK_STRING("enc")] : impl[MSDK_STRING("general")]; 
            sts = InitMFXSession( &m_pSessionEncode, implEnc);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            break;
        }

        default:
        {
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        }
    }

    return sts;

} // mfxStatus TranscodeModelReference::InitMFXSessions( SessionMode mode, mfxIMPL impl )


mfxStatus TranscodeModelReference::CloseMFXSessions( SessionMode mode )
{
    mfxStatus sts = MFX_ERR_NONE;

    switch( mode )
    {
        case DECVPPENC_SESSION:
        {            
            CloseMFXSession( m_pSessionDecode );

            break;
        }

        case DEC_VPP_ENC_SESSION:
        {
            CloseMFXSession( m_pSessionDecode );          

            CloseMFXSession( m_pSessionVPP );

            CloseMFXSession( m_pSessionEncode );            

            break;
        }

        case DECVPP_ENC_SESSION:
        {
            CloseMFXSession( m_pSessionDecode );  

            CloseMFXSession( m_pSessionEncode );            

            break;
        }

        case DECENC_VPP_SESSION:
        {
            CloseMFXSession( m_pSessionDecode ); 

            CloseMFXSession( m_pSessionVPP );          

            break;
        }

        case DEC_VPPENC_SESSION:
        {
            CloseMFXSession( m_pSessionDecode );             

            CloseMFXSession( m_pSessionVPP );

            break;
        }

        // join session
        case DECVPP_ENC_JOIN_SESSION:
        {
            CloseMFXSession( m_pSessionDecode );  

            CloseMFXSession( m_pSessionEncode ); 

            break;
        }

        case DEC_VPPENC_JOIN_SESSION:
        {
            CloseMFXSession( m_pSessionDecode );  

            CloseMFXSession( m_pSessionEncode ); 

            break;
        }

        case DEC_VPP_ENC_JOIN_SESSION:
        {
            CloseMFXSession( m_pSessionDecode );

            CloseMFXSession( m_pSessionVPP );

            CloseMFXSession( m_pSessionEncode );

            break;
        }

        default:
        {
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        }
    }

    m_pSessionDecode = m_pSessionVPP = m_pSessionEncode = NULL;

    return sts;

} // mfxStatus TranscodeModelReference::CloseMFXSessions( SessionMode mode )


// rules for join session: 
// (1) joining session: from left to right (ex: dec->vpp->enc)
// (2) left component is owner of parent session
mfxStatus TranscodeModelReference::JoinMFXSessions(SessionMode mode)
{
    mfxStatus sts;

    switch( mode )
    {
        case DECVPP_ENC_JOIN_SESSION:
        {
            sts = MFXJoinSession( *m_pSessionDecode, *m_pSessionEncode );
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            break;
        }

        case DEC_VPPENC_JOIN_SESSION:
        {
            sts = MFXJoinSession( *m_pSessionDecode, *m_pSessionEncode );
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            break;
        }

        case DEC_VPP_ENC_JOIN_SESSION:
        {
            sts = MFXJoinSession( *m_pSessionDecode, *m_pSessionVPP ); 
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = MFXJoinSession( *m_pSessionDecode, *m_pSessionEncode ); 
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            break;
        }

        default:
        {
            sts = MFX_ERR_NONE;
            break;
        }
    }

    return sts;

} // mfxStatus TranscodeModelReference::JoinMFXSessions(SessionMode mode)


mfxStatus TranscodeModelReference::DisjoinMFXSessions( SessionMode mode )
{
    mfxStatus sts;

    switch( mode )
    {
        case DECVPP_ENC_JOIN_SESSION:
        {
            sts = MFXDisjoinSession( *m_pSessionEncode );
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            break;
        }

        case DEC_VPPENC_JOIN_SESSION:
        {
            sts = MFXDisjoinSession( *m_pSessionEncode );
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            break;
        }

        case DEC_VPP_ENC_JOIN_SESSION:
        {
            sts = MFXDisjoinSession( *m_pSessionVPP ); 
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = MFXDisjoinSession( *m_pSessionEncode ); 
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            break;
        }

        default:
        {
            sts = MFX_ERR_NONE;
            break;
        }
    }

    return sts;
} // mfxStatus TranscodeModelReference::DisjoinMFXSessions( SessionMode mode )


bool TranscodeModelReference::IsJoinSessionEnable( void )
{
    bool sts;

    switch( m_sessionMode )
    {
        case DECVPP_ENC_JOIN_SESSION:
        case DEC_VPPENC_JOIN_SESSION:
        case DEC_VPP_ENC_JOIN_SESSION:
        {
            sts = true;
            break;
        }

        default:
        {
            sts = false;
            break;
        }
    }

    return sts;

} // bool TranscodeModelReference::IsJoinSessionEnable( void )


mfxStatus TranscodeModelReference::CreateAllocator( mfxU16 IOPattern )
{
    mfxStatus sts;

    m_allocator.pMfxAllocator = new GeneralAllocator;

    m_allocator.pAllocatorParams = NULL;

#if !defined (LIBVA_SUPPORT)
    if( IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY) )
    {
#else
        m_allocator.pLibVA = CreateLibVA(MFX_LIBVA_DRM);
        MSDK_CHECK_POINTER(m_allocator.pLibVA, MFX_ERR_NULL_PTR);
#endif
        sts = CreateEnvironmentHw( &m_allocator );
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#if !defined (LIBVA_SUPPORT)
    }
#endif

    sts = (m_allocator.pMfxAllocator)->Init(m_allocator.pAllocatorParams );

    return sts;
    
} // mfxStatus TranscodeModelReference::CreateAllocator( mfxU16 IOPattern )

void TranscodeModelReference::DestroyAllocator(void)
{ 
    MSDK_SAFE_DELETE(m_allocator.pMfxAllocator);
#if defined(LIBVA_SUPPORT)
    MSDK_SAFE_DELETE(m_allocator.pLibVA);
#endif
}// void TranscodeModelReference::DestroyAllocator(void)


mfxStatus TranscodeModelReference::SetAllocatorMFXSessions( mfxU16 IOPattern )
{
    mfxStatus sts = MFX_ERR_NONE;    

    if( IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY) ) // allocator need for Decode & VPP
    {
#if defined(_WIN32) || defined(_WIN64)
        // allocator need for Decode
        sts = m_pSessionDecode->SetHandle( MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, m_allocator.pd3dDeviceManager);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = m_pSessionDecode->SetFrameAllocator( m_allocator.pMfxAllocator ); 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // as well as for VPP        
        sts = m_pSessionVPP->SetHandle( MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, m_allocator.pd3dDeviceManager);
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = m_pSessionVPP->SetFrameAllocator( m_allocator.pMfxAllocator ); 
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
#elif defined (LIBVA_SUPPORT)
        // allocator need for Decode
        sts = m_pSessionDecode->SetFrameAllocator( m_allocator.pMfxAllocator ); 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // as well as for VPP        
        sts = m_pSessionVPP->SetFrameAllocator(m_allocator.pMfxAllocator ); 
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 

#endif
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if( IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY) ) // allocator need for VPP & Encode
    {
#if defined(_WIN32) || defined(_WIN64)
        // allocator need for VPP
        sts = m_pSessionVPP->SetHandle( MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, m_allocator.pd3dDeviceManager);
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = m_pSessionVPP->SetFrameAllocator( m_allocator.pMfxAllocator ); 
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // as well as for Encode
        sts = m_pSessionEncode->SetHandle( MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, m_allocator.pd3dDeviceManager);
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = m_pSessionEncode->SetFrameAllocator( m_allocator.pMfxAllocator );
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#elif defined (LIBVA_SUPPORT)
        // allocator need for VPP
        sts = m_pSessionVPP->SetFrameAllocator( m_allocator.pMfxAllocator ); 
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // as well as for Encode
        sts = m_pSessionEncode->SetFrameAllocator( m_allocator.pMfxAllocator );
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_UNDEFINED_BEHAVIOR); //(workaround if session called twice) 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#endif
    }

    return MFX_ERR_NONE;
} // mfxStatus TranscodeModelReference::SetAllocatorMFXSessions( mfxU16 IOPattern )


mfxStatus TranscodeModelReference::ReadBitstreamHeader(mfxVideoParam& bsHeaderParam)
{
    mfxStatus sts;     

    // try to find a sequence header in the stream
    // if header is not found this function exits with error (e.g. if device was lost and there's no header in the remaining stream)
    for(;;)
    {
        mfxBitstream* pBS = m_bsReader.GetBitstreamPtr();
        // parse bit stream and fill mfx param
        sts = m_pmfxDEC->DecodeHeader(pBS, &bsHeaderParam);

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (pBS->MaxLength == pBS->DataLength)
            {
                sts = m_bsReader.ExtendBitstream(); 
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            // read a portion of data             
            sts = m_bsReader.ReadNextFrame();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            continue;
        }
        else
        {
            break;
        }
    } // for()
    

    // check DecodeHeader status
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);        
    
    return sts;

} // mfxStatus ReadBitstreamHeader(mfxVideoParam& bsHeaderParam)


mfxStatus TranscodeModelReference::AllocEnoughBuffer(mfxBitstream* pBS)
{    
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    mfxStatus sts = m_pmfxENC->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(pBS, par.mfx.BufferSizeInKB * 1000); 
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(pBS));

    return MFX_ERR_NONE;

} // mfxStatus TranscodeModelReference::AllocEnoughBuffer(mfxBitstream* pBS)


mfxStatus TranscodeModelReference::Run()
{
    mfxStatus sts = MFX_ERR_NONE;
    bool IsCodedFrames = true; // indicates coded frames for decoder
    bool bNewDataRequired = false; // we have full input bitstream for decode in the beginning of pipeline

    mfxFrameSurfaceEx decSurfaceEx = {0};
    mfxFrameSurfaceEx vppSurfaceEx = {0};
    mfxFrameSurface1* pEncSurface = NULL;
    mfxBitstream *pBS = NULL;

    int decFramesCount = 0, vppFramesCount = 0, encFramesCount = 0;    

    while( MFX_ERR_NONE == sts ||  MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts )
    {
        // [reader]
        if( bNewDataRequired )
        {
            sts = ReadOneFrame(); 
            bNewDataRequired = false;
        }
        pBS = GetSrcPtr(); 

        // [decode]        
        // if need more input frames
        if (IsCodedFrames)
        {
            sts = DecodeOneFrame(pBS, 
                                 IsVPPEnable() ? &m_dec2vppSurfManager : &m_dec2encSurfManager, 
                                 &decSurfaceEx);
        }

        if( MFX_ERR_MORE_DATA == sts )
        {
            if( pBS )
            {
                bNewDataRequired = true; 
                continue;
            }
            else
            {          
                // to get buffered VPP or ENC frames
                decSurfaceEx.pSurface = NULL;
                sts = MFX_ERR_NONE;
                IsCodedFrames = false;
            }            
        }
        else if (MFX_ERR_NONE == sts) 
        {
            if( IsDecodeSyncOperationRequired( m_sessionMode ) )
            {
                if( decSurfaceEx.syncp )
                {
                    sts = SynchronizeDecode( decSurfaceEx.syncp );
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                }
            }
            if (IsCodedFrames)
            {
                //m_nProcessedFramesNum++;
                decFramesCount++;
            }
        }
        else
        {
            //printf("\nERROR\n");
            return sts; // error occurs
        }

        if( IsVPPEnable() )
        {
            // [vpp]
            sts = VPPOneFrame(decSurfaceEx.pSurface, &m_vpp2encSurfManager, &vppSurfaceEx);

            if (MFX_ERR_MORE_DATA == sts)
            {
                // to get buffered ENC frames
                vppSurfaceEx.pSurface = NULL;
                sts = MFX_ERR_NONE;
            }
            else if (MFX_ERR_NONE == sts)
            {
                if( IsVppSyncOperationRequired( m_sessionMode ) )
                {
                    if( vppSurfaceEx.syncp )
                    {
                        sts = SynchronizeVPP( vppSurfaceEx.syncp );
                        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    }
                }

                vppFramesCount++;
            }
            else
            {
                return sts; //error occurs
            }
        }

        // [encode]
        pBS = GetDstPtr();//critical section???
        // cast bitstream->bistreamEx
        mfxBitstreamEx bitstreamEx;
        // important! operator= creates copy of bitstream (pBS), so real object must be updated in case of modification of copy
        bitstreamEx.bitstream = *pBS; 
        bitstreamEx.syncp = 0;

        if( IsVPPEnable() )
        {
            pEncSurface = vppSurfaceEx.pSurface;
        }
        else
        {
            pEncSurface = decSurfaceEx.pSurface;
        }
        sts = EncodeOneFrame( pEncSurface, &bitstreamEx );

        // update real object in case of modification of copy
        if( bitstreamEx.bitstream.Data != pBS->Data )
        {
            pBS->Data = bitstreamEx.bitstream.Data;
        }

        // check if we need one more frame from decode
        if (MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts) 
        {
            if (NULL == pEncSurface ) // there are no more buffered frames in encoder
            {
                break;
            }
            else
            {
                // get next frame from Decode
                sts = MFX_ERR_NONE;
                continue;
            }
        }
        else if (MFX_ERR_NONE == sts)
        {
            encFramesCount++;
        }
        else
        {
            return sts; //error occurs
        }

        // check encoding result
        

        // [write BS]
        sts = PutBS( &bitstreamEx );
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    } // end of while

    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);

    // AYA debug
    //printf("\n decoded = %i, vpp = %i, encoded = %i\n", decFramesCount, vppFramesCount, encFramesCount);
    
    return sts;

} // mfxStatus TranscodeModelReference::Run()


/* ******************************************************************** */
/*                 Universal transcoding primitives                     */
/* ******************************************************************** */
mfxStatus TranscodeModelReference::ReadOneFrame( void )
{
    return m_bsReader.ReadNextFrame(); 

} // mfxStatus TranscodeModelReference::ReadOneFrame( void )


mfxBitstream* TranscodeModelReference::GetSrcPtr( void )
{
    return m_bsReader.GetBitstreamPtr();

} // mfxBitstream* GetSrcPtr( void )


mfxStatus TranscodeModelReference::DecodeOneFrame(mfxBitstream *pBS, 
                                           TUMSurfacesManager *pSurfacePool, 
                                           mfxFrameSurfaceEx *pSurfaceEx)
{
    MSDK_CHECK_POINTER(pSurfaceEx,  MFX_ERR_NULL_PTR);
    pSurfaceEx->pSurface = NULL;

    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    mfxFrameSurface1 *pmfxSurface = NULL;    
    mfxU32 i = 0;
    
    while (MFX_ERR_MORE_SURFACE == sts || 
           MFX_WRN_DEVICE_BUSY  == sts ||
           MFX_WRN_VIDEO_PARAM_CHANGED == sts)
    {        
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(5); // just wait and then repeat the same call to DecodeFrameAsync
        }        
        else if (MFX_ERR_MORE_SURFACE == sts)
        {
            // find new working surface  
            for (i = 0; i < MSDK_DEC_WAIT_INTERVAL; i += 5)
            {     
                 pmfxSurface= pSurfacePool->GetFreeSurface();// find new working surface 
                if (pmfxSurface)
                {
                    break;                    
                }
                else
                {
                    MSDK_SLEEP(5);                 
                }
            }        

            MSDK_CHECK_POINTER(pmfxSurface, MFX_ERR_MEMORY_ALLOC); // return an error if a free surface wasn't found
        }
        else if( MFX_WRN_VIDEO_PARAM_CHANGED != sts ) 
        {
            MSDK_BREAK_ON_ERROR(sts);
        }

        sts = m_pmfxDEC->DecodeFrameAsync(pBS, pmfxSurface, &pSurfaceEx->pSurface, &pSurfaceEx->syncp);        
        //MSDK_IGNORE_MFX_STS(sts, MFX_WRN_VIDEO_PARAM_CHANGED);

        // AYA: debug only
        if( pSurfaceEx->pSurface )
        {
            if( pSurfaceEx->pSurface->Info.PicStruct == 0 )
            {
                pSurfaceEx->pSurface->Info.PicStruct = 1;
            }
        }
        // AYA: final
          
    } //while processing
    
    return sts;    

} // mfxStatus CTranscodingPipeline::DecodeOneFrame(ExtendedSurface *pExtSurface)


mfxStatus TranscodeModelReference::VPPOneFrame(mfxFrameSurface1 *pSurfaceIn,
                                        TUMSurfacesManager *pSurfacePool,
                                        mfxFrameSurfaceEx *pSurfaceEx)
{
    MSDK_CHECK_POINTER(pSurfaceEx,  MFX_ERR_NULL_PTR);

    mfxFrameSurface1    *pmfxSurface = pSurfacePool->GetFreeSurface();// find new output surface 
    MSDK_CHECK_POINTER(pmfxSurface,  MFX_ERR_NULL_PTR);

    pSurfaceEx->pSurface = pmfxSurface;

    mfxStatus sts = MFX_ERR_NONE;
    for(;;)
    {
        sts = m_pmfxVPP->RunFrameVPPAsync(pSurfaceIn, pmfxSurface, NULL, &pSurfaceEx->syncp); 

        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(5); // just wait and then repeat the same call to RunFrameVPPAsync
        }
        else
        {
            break;
        }
    }             
    return sts;

} // mfxStatus TranscodeModelReference::VPPOneFrame(...)


mfxStatus TranscodeModelReference::EncodeOneFrame(mfxFrameSurface1 *pSurface, mfxBitstreamEx *pBSEx)
{
    mfxStatus sts = MFX_ERR_NONE;
    for (;;)
    {
        // at this point surface for encoder contains either a frame from file or a frame processed by vpp        
        sts = m_pmfxENC->EncodeFrameAsync(NULL, pSurface, &(pBSEx->bitstream), &pBSEx->syncp);
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(5);
        }
        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            sts = AllocEnoughBuffer( &(pBSEx->bitstream ));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);                
        }
        else
        {
            break;
        }
    }
    return sts;

} // mfxStatus TranscodeModelReference::EncodeOneFrame(...)


mfxStatus TranscodeModelReference::PutBS( mfxBitstreamEx *pBSEx )
{
    mfxStatus sts;

    MSDK_CHECK_POINTER(pBSEx, MFX_ERR_NULL_PTR);

    if( 0 == pBSEx->syncp )
    {
        return MFX_ERR_NONE;
    }

    // get result coded stream
    sts = SynchronizeEncode(pBSEx->syncp);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = WriteOneFrame( &(pBSEx->bitstream) );
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
   
    return sts;

} //mfxStatus CTranscodingPipeline::PutBS( mfxBitstreamEx *pBSEx )


mfxStatus TranscodeModelReference::WriteOneFrame( mfxBitstream* pBS )
{
    //printf("transcoded frame: %i\n", m_framesCount++);
    m_framesCount++;

    return m_bsWriter.WriteNextFrame( pBS );

} // mfxStatus TranscodeModelReference::WriteOneFrame( void )


mfxBitstream* TranscodeModelReference::GetDstPtr( void )
{
    return m_bsWriter.GetBitstreamPtr();

} // mfxBitstream* GetDstPtr( void )


mfxStatus TranscodeModelReference::SynchronizeDecode( mfxSyncPoint syncp )
{
    mfxStatus sts = m_pSessionDecode->SyncOperation(syncp, TUM_WAIT_INTERVAL);

    return sts;

} // mfxStatus TranscodeModelReference::SynchronizeDecode( mfxSyncPoint syncp )


mfxStatus TranscodeModelReference::SynchronizeVPP( mfxSyncPoint syncp )
{
    mfxStatus sts = m_pSessionVPP->SyncOperation(syncp, TUM_WAIT_INTERVAL);

    return sts;

} // mfxStatus TranscodeModelReference::SynchronizeVPP( mfxSyncPoint syncp )


mfxStatus TranscodeModelReference::SynchronizeEncode( mfxSyncPoint syncp )
{
    mfxStatus sts = m_pSessionEncode->SyncOperation(syncp, TUM_WAIT_INTERVAL);

    return sts;

} // mfxStatus TranscodeModelReference::SynchronizeEncode( mfxSyncPoint syncp )


/* ******************************************************************** */
/*            implementation of Behaviour functions                     */
/* ******************************************************************** */

bool TranscodeModelReference::IsDecodeSyncOperationRequired( SessionMode mode )
{
    bool sts;

    switch( mode )
    {
        case DEC_VPP_ENC_SESSION:        
        case DEC_VPPENC_SESSION:
        {
            sts = true;
            break;
        }
        case DECENC_VPP_SESSION:
        {
            if( IsVPPEnable() )
            {
                sts = true;
            }
            else
            {
                sts = false;
            }
            break;
        }
        default:
        {
            sts = false;
            break;
        }
    }

    return sts;

} // bool   IsEncodeSyncOperationRequired( SessionMode mode )


bool  TranscodeModelReference::IsVppSyncOperationRequired( SessionMode mode )
{
    bool sts;

    if( !IsVPPEnable() )
    {
        return false;
    }

    switch( mode )
    {
        case DEC_VPP_ENC_SESSION:
        case DECVPP_ENC_SESSION:
        case DECENC_VPP_SESSION:
        {
            sts = true;
            break;
        }
        default:
        {
            sts = false;
            break;
        }
    }

    return sts;

} // bool   IsVppSyncOperationRequired( SessionMode mode )

/* ******************************************************************** */
/*             implementation of service functions                      */
/* ******************************************************************** */

mfxStatus ConfigDecodeParam( AppParam param, mfxVideoParam bsHeaderParam, mfxVideoParam& decodeVideoParam)
{
    param;// warning disable

    decodeVideoParam = bsHeaderParam;

    // specify memory type 
    decodeVideoParam.IOPattern = param.IOPattern & (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY);
    decodeVideoParam.IOPattern = InvertIOPattern( decodeVideoParam.IOPattern );

    decodeVideoParam.mfx.DecodedOrder = 0; // binary flag, 0 signals decoder to output frames in display order

    // doesn't work with protected content
    decodeVideoParam.Protected = 0;

    return MFX_ERR_NONE;

} // mfxStatus ConfigDecodeParam(...) 


mfxStatus CreateVppExtension( AppParam param, mfxVideoParam& vppVideoParam )
{
    param;

    // configuration of DO_NOT_USE hint (filters)
    mfxExtVPPDoNotUse* pExtDoNotUse = new mfxExtVPPDoNotUse;    

    pExtDoNotUse->Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    pExtDoNotUse->Header.BufferSz = sizeof(mfxExtVPPDoNotUse);
    pExtDoNotUse->NumAlg = 3;

    pExtDoNotUse->AlgList = new mfxU32 [pExtDoNotUse->NumAlg];    

    pExtDoNotUse->AlgList[0] = MFX_EXTBUFF_VPP_DENOISE;        // turn off denoising (on by default)
    pExtDoNotUse->AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS; // turn off scene analysis (on by default)
    pExtDoNotUse->AlgList[2] = MFX_EXTBUFF_VPP_DETAIL;         // turn off detail enhancement (on by default)

    // allocate external buffers array
    vppVideoParam.ExtParam    = new mfxExtBuffer* [1];     
    vppVideoParam.ExtParam[0] = (mfxExtBuffer *)pExtDoNotUse;
    vppVideoParam.NumExtParam = 1; 

    return MFX_ERR_NONE;

} // mfxStatus CreateVppExtension( AppParam param, mfxVideoParam& vppVideoParam )


mfxStatus DestroyVppExtension( mfxVideoParam& vppVideoParam )
{
    if ( vppVideoParam.ExtParam )
    {
        mfxExtVPPDoNotUse* pExtDoNotUse = (mfxExtVPPDoNotUse* )( vppVideoParam.ExtParam[0] );

        MSDK_SAFE_DELETE_ARRAY( pExtDoNotUse->AlgList );
        MSDK_SAFE_DELETE( pExtDoNotUse );                
        MSDK_SAFE_DELETE_ARRAY( vppVideoParam.ExtParam ); 
    }          
    
    return MFX_ERR_NONE;

} // mfxStatus DestroyVppExtension( mfxVideoParam vppVideoParam )


mfxStatus ConfigVppParam( AppParam param, mfxVideoParam decodeVideoParam, mfxVideoParam& vppVideoParam)
{

    vppVideoParam.IOPattern = param.IOPattern;

    // [INPUT]
    vppVideoParam.vpp.In.FourCC    = MFX_FOURCC_NV12;    
    vppVideoParam.vpp.In.PicStruct = decodeVideoParam.mfx.FrameInfo.PicStruct;

    // frame rate from src bitstream is optional.
    /*if ( decodeVideoParam.mfx.FrameInfo.FrameRateExtN > 0 &&  decodeVideoParam.mfx.FrameInfo.FrameRateExtD > 0 )
    {
        vppVideoParam.vpp.In.FrameRateExtN = decodeVideoParam.mfx.FrameInfo.FrameRateExtN; 
        vppVideoParam.vpp.In.FrameRateExtD = decodeVideoParam.mfx.FrameInfo.FrameRateExtD; 
    }
    else*/
    {
        ConvertFrameRate(param.frameRate, 
                     &(vppVideoParam.vpp.In.FrameRateExtN), 
                     &(vppVideoParam.vpp.In.FrameRateExtD) );
    }
    
    vppVideoParam.vpp.In.Width = decodeVideoParam.mfx.FrameInfo.Width; 
    vppVideoParam.vpp.In.Height= (MFX_PICSTRUCT_PROGRESSIVE == vppVideoParam.vpp.In.PicStruct)?
                                     MSDK_ALIGN16(decodeVideoParam.mfx.FrameInfo.Height) : MSDK_ALIGN32(decodeVideoParam.mfx.FrameInfo.Height);
    
    vppVideoParam.vpp.In.CropW = decodeVideoParam.mfx.FrameInfo.CropW;
    vppVideoParam.vpp.In.CropH = decodeVideoParam.mfx.FrameInfo.CropH;


    // [OUTPUT]
    vppVideoParam.vpp.Out = vppVideoParam.vpp.In;
    vppVideoParam.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420; // need for encode

    ConvertFrameRate(param.frameRate, 
                     &(vppVideoParam.vpp.Out.FrameRateExtN), 
                     &(vppVideoParam.vpp.Out.FrameRateExtD) );

    // only resizing is supported
    if( param.width > 0 && param.height > 0 )
    {
        vppVideoParam.vpp.Out.CropW = param.width;
        vppVideoParam.vpp.Out.CropH = param.height;

        vppVideoParam.vpp.Out.Width = MSDK_ALIGN16(param.width);
        vppVideoParam.vpp.Out.Height= (MFX_PICSTRUCT_PROGRESSIVE == vppVideoParam.vpp.Out.PicStruct)?
                                       MSDK_ALIGN16(param.height) : MSDK_ALIGN32(param.height); 
    }

    CreateVppExtension( param, vppVideoParam );

    // doesn't work with protected content
    vppVideoParam.Protected = 0;

    return MFX_ERR_NONE;

} // mfxStatus ConfigVppParam(...)


mfxStatus ConfigEncodeParam( AppParam param, mfxVideoParam vppVideoParam, mfxVideoParam& encodeVideoParam)
{
    encodeVideoParam.mfx.NumThread             = 0;//pInParams->nThreads; 0 - encode make decision

    encodeVideoParam.mfx.CodecId               = param.dstVideoFormat;
    encodeVideoParam.mfx.TargetUsage           = param.targetUsage; // trade-off between quality and speed
    encodeVideoParam.mfx.RateControlMethod     = MFX_RATECONTROL_CBR;
    
    encodeVideoParam.mfx.FrameInfo.FrameRateExtN = vppVideoParam.vpp.Out.FrameRateExtN;
    encodeVideoParam.mfx.FrameInfo.FrameRateExtD = vppVideoParam.vpp.Out.FrameRateExtD;
    
    encodeVideoParam.mfx.EncodedOrder            = 0; // binary flag, 0 signals encoder to take frames in display order

    encodeVideoParam.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
    encodeVideoParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    encodeVideoParam.mfx.FrameInfo.PicStruct    = vppVideoParam.vpp.Out.PicStruct;
    
    encodeVideoParam.mfx.FrameInfo.Width  = vppVideoParam.vpp.Out.Width;
    encodeVideoParam.mfx.FrameInfo.Height = vppVideoParam.vpp.Out.Height;

    encodeVideoParam.mfx.FrameInfo.CropX  = 0; 
    encodeVideoParam.mfx.FrameInfo.CropY  = 0;
    encodeVideoParam.mfx.FrameInfo.CropW  = vppVideoParam.vpp.Out.CropW;
    encodeVideoParam.mfx.FrameInfo.CropH  = vppVideoParam.vpp.Out.CropH;    
  
    // calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
    if (param.bitRate == 0)
    {        
        param.bitRate = CalculateDefaultBitrate(encodeVideoParam.mfx.CodecId, 
                                                encodeVideoParam.mfx.TargetUsage, 
                                                encodeVideoParam.mfx.FrameInfo.Width,
                                                encodeVideoParam.mfx.FrameInfo.Height, 
                                                param.frameRate);
    }

    encodeVideoParam.mfx.TargetKbps = (mfxU16)(param.bitRate); // in Kbps   
      
    encodeVideoParam.IOPattern = param.IOPattern & (MFX_IOPATTERN_OUT_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY);
    encodeVideoParam.IOPattern = InvertIOPattern( encodeVideoParam.IOPattern );

    encodeVideoParam.mfx.NumSlice = param.NumSlice;

    // doesn't work with protected content
    encodeVideoParam.Protected = 0;
    
    // we don't specify profile and level and let the encoder choose those basing on parameters   
    return MFX_ERR_NONE;

}// mfxStatus ConfigEncodeParam(...)
/* EOF */
