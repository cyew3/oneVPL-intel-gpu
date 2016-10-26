//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#include "mfx_vpp_utils.h"
#include "mfx_frame_rate_conversion_vpp.h"

/* ******************************************************************** */
/*                 implementation of VPP filter [FrameRateConversion]   */
/* ******************************************************************** */

mfxStatus MFXVideoVPPFrameRateConversion::Query( mfxExtBuffer* pHint )
{
    if( NULL == pHint )
    {
        return MFX_ERR_NONE;
    }


    mfxExtVPPFrameRateConversion* pParam = (mfxExtVPPFrameRateConversion*)pHint;

    if( MFX_FRCALGM_PRESERVE_TIMESTAMP    == pParam->Algorithm    || 
        MFX_FRCALGM_DISTRIBUTED_TIMESTAMP == pParam->Algorithm ||
        MFX_FRCALGM_FRAME_INTERPOLATION   == pParam->Algorithm)
    {
        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }    

} // mfxStatus MFXVideoVPPFrameRateConversion::Query( mfxExtBuffer* pHint )

#if !defined(MFX_ENABLE_HW_ONLY_VPP) // SW ONLY

MFXVideoVPPFrameRateConversion::MFXVideoVPPFrameRateConversion(VideoCORE *core, mfxStatus* sts) : FilterVPP()
{

    m_core = core;

    m_deltaTimeStamp= 0;
    m_timeStamp     = 0;

    m_inFrameTime  = 0.0;
    m_outFrameTime = 0.0;
    m_timeFrameInterval = 0.0;    

    m_pRefSurface[0] = NULL;  
    m_pRefSurface[1] = NULL;  

    ResetNativeState( &m_nativeSyncState );
    ResetNativeState( &m_nativeProcessState );


    m_bAdvancedMode = false;

    m_minDeltaTime = 0;

    ResetAdvancedState( &m_advSyncState );
    ResetAdvancedState( &m_advProcessState );    

    VPP_CLEAN;

    *sts = MFX_ERR_NONE;

    return;

} // MFXVideoVPPFrameRateConversion::MFXVideoVPPFrameRateConversion(...)


MFXVideoVPPFrameRateConversion::~MFXVideoVPPFrameRateConversion(void)
{
    Close();

    return;

} // MFXVideoVPPFrameRateConversion::~MFXVideoVPPFrameRateConversion(void) 


mfxStatus MFXVideoVPPFrameRateConversion::SetParam( mfxExtBuffer* pHint )
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    if( pHint )
    {
        mfxExtVPPFrameRateConversion* pParam = (mfxExtVPPFrameRateConversion*)pHint;
        if( MFX_FRCALGM_DISTRIBUTED_TIMESTAMP == pParam->Algorithm )
        {
            m_bAdvancedMode = true;
        }
        else if(MFX_FRCALGM_PRESERVE_TIMESTAMP == pParam->Algorithm)
        {
            m_bAdvancedMode = false;
        }
        else
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    if( m_bAdvancedMode )
    {

        m_minDeltaTime = IPP_MIN((mfxU64) (m_errPrtctState.In.FrameRateExtD * MFX_TIME_STAMP_FREQUENCY) / (2 * m_errPrtctState.In.FrameRateExtN), 
                               (mfxU64) (m_errPrtctState.Out.FrameRateExtD * MFX_TIME_STAMP_FREQUENCY) / (2 * m_errPrtctState.Out.FrameRateExtN));
    }

    return mfxSts;

} // mfxStatus MFXVideoVPPFrameRateConversion::SetParam( mfxExtBuffer* pHint )


mfxStatus MFXVideoVPPFrameRateConversion::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1( par );

    VPP_CHECK_NOT_INITIALIZED;

    ResetNativeState( &m_nativeSyncState );
    ResetNativeState( &m_nativeProcessState );

    if( m_pRefSurface[0] )
    {
        mfxStatus sts = m_core->DecreaseReference( &(m_pRefSurface[0]->Data) );
        MFX_CHECK_STS( sts );       
    }   
    m_pRefSurface[0] = NULL;

    if( m_pRefSurface[1] )
    {
        mfxStatus sts = m_core->DecreaseReference( &(m_pRefSurface[1]->Data) );
        MFX_CHECK_STS( sts );       
    }
    m_pRefSurface[1] = NULL;

    VPP_RESET;

    m_errPrtctState.In = par->vpp.In;
    m_errPrtctState.Out= par->vpp.Out;

    mfxExtBuffer* pHint = NULL;

    GetFilterParam(par, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, &pHint);

    SetParam( pHint );

    // m_bAdvancedMode = false; // it is right: after reset VPP should remember about initialization mode 
    ResetAdvancedState( &m_advSyncState );
    ResetAdvancedState( &m_advProcessState );    

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPFrameRateConversion::Reset(mfxVideoParam *par) 


mfxStatus MFXVideoVPPFrameRateConversion::RunFrameVPP(mfxFrameSurface1 *in, 
                                                      mfxFrameSurface1 *out, 
                                                      InternalParam* pParam)
{
    mfxStatus sts;

    VPP_CHECK_NOT_INITIALIZED;   

    // processing: error 
    if( NULL == out )
    {
        return MFX_ERR_NULL_PTR;
    }
    if( NULL == pParam )
    {
        return MFX_ERR_NULL_PTR;
    }
    
    if( (NULL == in) && (!IsReadyOutput(MFX_REQUEST_FROM_VPP_PROCESS)) )
    {
        return MFX_ERR_MORE_DATA;
    }
    
    if(m_bAdvancedMode)
    {
        sts = RunFrameVPP_AdvancedFRC(in, out, pParam);        
    }
    else
    {
        sts = RunFrameVPP_NativeFRC(in, out, pParam);        
    }
    
    return sts;

} // mfxStatus MFXVideoVPPFrameRateConversion::RunFrameVPP(...)


mfxStatus MFXVideoVPPFrameRateConversion::RunFrameVPP_NativeFRC(mfxFrameSurface1 *in, 
                                                                mfxFrameSurface1 *out, 
                                                                InternalParam* pParam)
{
    mfxStatus sts;

    mfxF64 deltaTime = m_nativeProcessState.deltaTime + m_timeFrameInterval;

    if (deltaTime <= -m_outFrameTime)
    {
        m_nativeProcessState.deltaTime   += m_inFrameTime;
        m_nativeProcessState.bReadyOutput = false;

        if( m_pRefSurface[0] )
        {
            sts = m_core->DecreaseReference( &(m_pRefSurface[0]->Data) );
            MFX_CHECK_STS( sts );
        }
        m_pRefSurface[0] = NULL;

        // skip frame
        // request new one input surface
        return MFX_ERR_MORE_DATA;
    }
    else if (deltaTime >= m_outFrameTime)
    {
        m_nativeProcessState.deltaTime    += -m_outFrameTime;
        m_nativeProcessState.bReadyOutput  = true;

        if( m_pRefSurface[0] )
        {
            bool bROIControl = IsROIConstant(out, out, m_pRefSurface[0]);

            sts = SurfaceCopy_ROI(out, m_pRefSurface[0], bROIControl);
            MFX_CHECK_STS( sts );            
        }
        else
        {
            MFX_CHECK_NULL_PTR1( in );
            
            m_pRefSurface[0]  = in;
            m_nativeProcessState.refPicStruct = pParam->inPicStruct;

            sts = m_core->IncreaseReference( &(m_pRefSurface[0]->Data) );
            MFX_CHECK_STS( sts );

            bool bROIControl = IsROIConstant(out, out, m_pRefSurface[0]);

            sts = SurfaceCopy_ROI(out, m_pRefSurface[0], bROIControl);
            MFX_CHECK_STS( sts );            
        }

        pParam->outPicStruct = m_nativeProcessState.refPicStruct;

        // duplicate this frame
        // request new one output surface
        return MFX_ERR_MORE_SURFACE;
    }
    else
    {
        m_nativeProcessState.deltaTime += m_timeFrameInterval;
        m_nativeProcessState.bReadyOutput = false;

        if( m_pRefSurface[0] )
        {
            bool bROIControl = IsROIConstant(out, out, m_pRefSurface[0]);

            sts = SurfaceCopy_ROI(out, m_pRefSurface[0], bROIControl);
            MFX_CHECK_STS( sts );

            pParam->outPicStruct = m_nativeProcessState.refPicStruct;

            sts = m_core->DecreaseReference( &(m_pRefSurface[0]->Data) );
            MFX_CHECK_STS( sts );

            m_pRefSurface[0] = NULL;
        }
        else
        {
            MFX_CHECK_NULL_PTR1( in );

            sts = SurfaceCopy_ROI(out, in);
            MFX_CHECK_STS( sts );

            pParam->outPicStruct = pParam->inPicStruct;
        }

        //produce frame
        return MFX_ERR_NONE;
    }

} // mfxStatus MFXVideoVPPFrameRateConversion::RunFrameVPP_NativeFRC(...)


mfxStatus MFXVideoVPPFrameRateConversion::RunFrameVPP_AdvancedFRC(mfxFrameSurface1 *in, 
                                                                  mfxFrameSurface1 *out, 
                                                                  InternalParam* pParam)
{
    AdvancedFRCState* ptr = &m_advProcessState;
    mfxStatus sts;

    //------------------------------------------------
    //           ReadyOutput
    //------------------------------------------------
    if( ptr->bReadyOutput )
    {
        ptr->expectedTimeStamp = GetExpectedPTS(ptr->numOutputFrames, ptr->timeOffset, ptr->timeStampJump);

        MFX_CHECK_NULL_PTR1( m_pRefSurface[1] );

        {
            bool bROIControl = IsROIConstant(out, out, m_pRefSurface[1]);

            sts = SurfaceCopy_ROI(out, m_pRefSurface[1], bROIControl);
            MFX_CHECK_STS( sts );            
        }
        
        mfxU64 nextExpectedTimeStamp = GetExpectedPTS(ptr->numOutputFrames + 1, ptr->timeOffset, ptr->timeStampJump);

        if( (ptr->defferedInputTimeStamp > nextExpectedTimeStamp) || (mfxU64)(abs((mfxI32)(ptr->defferedInputTimeStamp - nextExpectedTimeStamp))) < m_minDeltaTime )
        {
            pParam->outPicStruct = ptr->refPicStruct[1];

            sts = MFX_ERR_MORE_SURFACE;            
        }
        else
        {
            ptr->bReadyOutput = false;
            pParam->outPicStruct = ptr->refPicStruct[0];

            sts = m_core->DecreaseReference( &(m_pRefSurface[1]->Data) );
            MFX_CHECK_STS( sts );

            m_pRefSurface[1] = NULL;

            sts = MFX_ERR_NONE;
        }

        pParam->outTimeStamp =  ptr->expectedTimeStamp;
        ptr->numOutputFrames++;

        return sts;
    }

    //------------------------------------------------
    //           standard processing
    //------------------------------------------------
    MFX_CHECK_NULL_PTR1( in );
    mfxU64 inputTimeStamp = pParam->inTimeStamp;

    if( false == ptr->bIsSetTimeOffset )
    {
        ptr->bIsSetTimeOffset = true;
        ptr->timeOffset = inputTimeStamp;
    }

    // calculate expected timestamp based on output framerate
    ptr->expectedTimeStamp = GetExpectedPTS(ptr->numOutputFrames, ptr->timeOffset, ptr->timeStampJump); 

    mfxU32 timeStampDifference = abs((mfxI32)(inputTimeStamp - ptr->expectedTimeStamp));

    // process irregularity
    if (m_minDeltaTime > timeStampDifference)
    {
        inputTimeStamp = ptr->expectedTimeStamp;
    }

    // made decision regarding frame rate conversion
    if ( inputTimeStamp < ptr->expectedTimeStamp )
    {
        ptr->bReadyOutput     = false;

        if( m_pRefSurface[0] )
        {
            sts = m_core->DecreaseReference( &(m_pRefSurface[0]->Data) );
            MFX_CHECK_STS( sts );
        }
        m_pRefSurface[0] = in;
        sts = m_core->IncreaseReference( &(m_pRefSurface[0]->Data) );
        MFX_CHECK_STS( sts );

        ptr->refPicStruct[0] = pParam->inPicStruct;

        // skip frame
        // request new one input surface
        return MFX_ERR_MORE_DATA;
    }
    else if ( inputTimeStamp == ptr->expectedTimeStamp ) // see above (minDelta)
    {        
        ptr->bReadyOutput     = false;                

        if( m_pRefSurface[1] )
        {
            sts = m_core->DecreaseReference( &(m_pRefSurface[1]->Data) );
            MFX_CHECK_STS( sts );

            m_pRefSurface[1] = NULL;
        }

        if( m_pRefSurface[0] )
        {
            sts = m_core->DecreaseReference( &(m_pRefSurface[0]->Data) );
            MFX_CHECK_STS( sts );
        }

        m_pRefSurface[0] = in;
        sts = m_core->IncreaseReference( &(m_pRefSurface[0]->Data) );
        MFX_CHECK_STS( sts );

        sts = SurfaceCopy_ROI(out, in);
        MFX_CHECK_STS( sts );

        ptr->refPicStruct[0] = pParam->inPicStruct;

        pParam->outPicStruct = pParam->inPicStruct;

        pParam->outTimeStamp = ptr->expectedTimeStamp;

        ptr->numOutputFrames++;

        return MFX_ERR_NONE;
    }
    else // inputTimeStampParam > ptr->expectedTimeStamp
    {
        if( m_pRefSurface[1] )
        {
            sts = m_core->DecreaseReference( &(m_pRefSurface[1]->Data) );
            MFX_CHECK_STS( sts );            
        }

        m_pRefSurface[1] = m_pRefSurface[0];
        ptr->refPicStruct[1] = ptr->refPicStruct[0];
        
        sts = SurfaceCopy_ROI(out, m_pRefSurface[0]);
        MFX_CHECK_STS( sts );

        m_pRefSurface[0]      = in; 

        sts = m_core->IncreaseReference( &(m_pRefSurface[0]->Data) );
        MFX_CHECK_STS( sts );            

        ptr->defferedInputTimeStamp = inputTimeStamp;

        mfxU64 nextExpectedTimeStamp = GetExpectedPTS(ptr->numOutputFrames + 1, ptr->timeOffset, ptr->timeStampJump);

        if( (ptr->defferedInputTimeStamp > nextExpectedTimeStamp) || (mfxU64)(abs((mfxI32)(ptr->defferedInputTimeStamp - nextExpectedTimeStamp))) < m_minDeltaTime )
        {
            ptr->bReadyOutput = true;

            pParam->outPicStruct = ptr->refPicStruct[0];
            ptr->refPicStruct[1] = ptr->refPicStruct[0];
            ptr->refPicStruct[0] = pParam->inPicStruct;

            sts = MFX_ERR_MORE_SURFACE;
        }
        else
        {
            ptr->bReadyOutput = false; 

            pParam->outPicStruct = pParam->inPicStruct;
            ptr->refPicStruct[0] = pParam->inPicStruct;

            sts = MFX_ERR_NONE;
        }        

        pParam->outTimeStamp = ptr->expectedTimeStamp;

        ptr->numOutputFrames++;

        return sts;
    }
    
} // mfxStatus MFXVideoVPPFrameRateConversion::RunFrameVPP_AdvancedFRC(...)


mfxStatus MFXVideoVPPFrameRateConversion::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
    MFX_CHECK_NULL_PTR2( In, Out );

    VPP_CHECK_MULTIPLE_INIT;

    ResetNativeState( &m_nativeSyncState );
    ResetNativeState( &m_nativeProcessState );

    m_bAdvancedMode = false;
    ResetAdvancedState( &m_advSyncState );
    ResetAdvancedState( &m_advProcessState );

    m_pRefSurface[0] = NULL;

    m_errPrtctState.In = *In;
    m_errPrtctState.Out= *Out;

    m_timeStamp = 0;
    m_deltaTimeStamp = (Out->FrameRateExtD * MFX_TIME_STAMP_FREQUENCY)/ Out->FrameRateExtN;

    // calculate time of one frame in ms
    m_inFrameTime = 1000.0 / ((mfxF64)In->FrameRateExtN / (mfxF64)In->FrameRateExtD);
    m_outFrameTime = 1000.0 / ((mfxF64)Out->FrameRateExtN / (mfxF64)Out->FrameRateExtD);

    // calculate time interval between input and output frames
    m_timeFrameInterval = m_inFrameTime - m_outFrameTime; 

        
    VPP_INIT_SUCCESSFUL;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPFrameRateConversion::Init(mfxFrameInfo* In, mfxFrameInfo* Out)


mfxStatus MFXVideoVPPFrameRateConversion::Close(void)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    VPP_CHECK_NOT_INITIALIZED;

    ResetNativeState( &m_nativeSyncState );
    ResetNativeState( &m_nativeProcessState );

    m_timeStamp = 0;
    m_deltaTimeStamp = 0;

    m_inFrameTime = 0.0;
    m_outFrameTime= 0.0;
    m_timeFrameInterval= 0.0;  
    
    m_bAdvancedMode = false;
    ResetAdvancedState( &m_advSyncState );
    ResetAdvancedState( &m_advProcessState );

    VPP_CLEAN;

    return mfxSts;

} // mfxStatus MFXVideoVPPFrameRateConversion::Close(void)


// work buffer management - nothing for CSC filter
mfxStatus MFXVideoVPPFrameRateConversion::GetBufferSize( mfxU32* pBufferSize )
{
    VPP_CHECK_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(pBufferSize);

    *pBufferSize = 0;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPFrameRateConversion::GetBufferSize( mfxU32* pBufferSize )


mfxStatus MFXVideoVPPFrameRateConversion::SetBuffer( mfxU8* pBuffer )
{
    mfxU32 bufSize = 0;
    mfxStatus sts;

    VPP_CHECK_NOT_INITIALIZED;

    sts = GetBufferSize( &bufSize );
    MFX_CHECK_STS( sts );

    // FRC dosn't require work buffer, so pBuffer == NULL is OK
    if( bufSize )
    {
        MFX_CHECK_NULL_PTR1(pBuffer);
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPFrameRateConversion::SetBuffer( mfxU8* pBuffer )


// function is called from sync part of VPP only
mfxU64    MFXVideoVPPFrameRateConversion::UpdatePTS( mfxFrameSurface1 *in )
{
    if( IsReadyOutput(MFX_REQUEST_FROM_VPP_CHECK) )
    {
        m_timeStamp = MFX_TIME_STAMP_INVALID;// ISV3 make decision
    }
    else
    {            
        if( in ) // in != NULL here, but KW detected issue
        {
            m_timeStamp = in->Data.TimeStamp;
        }
        else
        {
            m_timeStamp = MFX_TIME_STAMP_INVALID;
        }
    }

    return m_timeStamp;

} // mfxU64    MFXVideoVPPFrameRateConversion::UpdatePTS( mfxFrameSurface1 *in )


mfxU32  MFXVideoVPPFrameRateConversion::UpdateFrameOrder( mfxFrameSurface1 *in )
{
    mfxU32 frameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;

    if( IsReadyOutput(MFX_REQUEST_FROM_VPP_CHECK) )
    {       
        frameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;// ISV3 make decision
    }
    else
    {           
        if( in ) // in != NULL here, but KW detected issue
        {
            frameOrder = in->Data.FrameOrder;
        }
        else
        {
            frameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
        }
    }

    return frameOrder;

} // mfxU32  MFXVideoVPPFrameRateConversion::UpdateFrameOrder( mfxFrameSurface1 *in )


mfxStatus MFXVideoVPPFrameRateConversion::CheckProduceOutput_NativeFRC(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
{
    mfxF64 deltaTime = m_nativeSyncState.deltaTime + m_timeFrameInterval;

    if (deltaTime <= -m_outFrameTime)
    {
        m_nativeSyncState.deltaTime   += m_inFrameTime;
        m_nativeSyncState.bReadyOutput = false;

        // skip frame
        // request new one input surface
        return MFX_ERR_MORE_DATA;
    }
    else if (deltaTime >= m_outFrameTime)
    {
        m_nativeSyncState.deltaTime    += -m_outFrameTime;        

        out->Data.TimeStamp  = UpdatePTS( in );
        out->Data.FrameOrder = UpdateFrameOrder( in );

        m_nativeSyncState.bReadyOutput  = true;

        if( in )
        {
            // pass through in according with new spec requirement
            out->Info.AspectRatioH = in->Info.AspectRatioH;
            out->Info.AspectRatioW = in->Info.AspectRatioW;
            out->Info.PicStruct    = in->Info.PicStruct;

            m_nativeSyncState.defferedFrameInfo.AspectRatioH = out->Info.AspectRatioH;
            m_nativeSyncState.defferedFrameInfo.AspectRatioW = out->Info.AspectRatioW;
            m_nativeSyncState.defferedFrameInfo.PicStruct    = out->Info.PicStruct;

        }
        else
        {
            out->Info.AspectRatioH = m_nativeSyncState.defferedFrameInfo.AspectRatioH;
            out->Info.AspectRatioW = m_nativeSyncState.defferedFrameInfo.AspectRatioW;
            out->Info.PicStruct    = m_nativeSyncState.defferedFrameInfo.PicStruct;
        }

        // duplicate this frame
        // request new one output surface
        return MFX_ERR_MORE_SURFACE;
    }
    else
    {
        m_nativeSyncState.deltaTime += m_timeFrameInterval;

        out->Data.TimeStamp  = UpdatePTS( in );
        out->Data.FrameOrder = UpdateFrameOrder( in );

        m_nativeSyncState.bReadyOutput = false;

        if( in )
        {
            // pass through in according with new spec requirement
            out->Info.AspectRatioH = in->Info.AspectRatioH;
            out->Info.AspectRatioW = in->Info.AspectRatioW;
            out->Info.PicStruct    = in->Info.PicStruct;

            m_nativeSyncState.defferedFrameInfo.AspectRatioH = out->Info.AspectRatioH;
            m_nativeSyncState.defferedFrameInfo.AspectRatioW = out->Info.AspectRatioW;
            m_nativeSyncState.defferedFrameInfo.PicStruct    = out->Info.PicStruct;

        }
        else
        {
            out->Info.AspectRatioH = m_nativeSyncState.defferedFrameInfo.AspectRatioH;
            out->Info.AspectRatioW = m_nativeSyncState.defferedFrameInfo.AspectRatioW;
            out->Info.PicStruct    = m_nativeSyncState.defferedFrameInfo.PicStruct;
        }

        //produce frame
        return MFX_ERR_NONE;
    }

} // mfxStatus MFXVideoVPPFrameRateConversion::CheckProduceOutput_NativeFRC(...)


mfxStatus MFXVideoVPPFrameRateConversion::CheckProduceOutput_AdvancedFRC(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
{
    AdvancedFRCState* ptr = &m_advSyncState;
    mfxStatus sts;

    //------------------------------------------------
    //           ReadyOutput
    //------------------------------------------------
    if( ptr->bReadyOutput )
    {
        ptr->expectedTimeStamp = GetExpectedPTS(ptr->numOutputFrames, ptr->timeOffset, ptr->timeStampJump);        
                
        mfxU64 nextExpectedTimeStamp = GetExpectedPTS(ptr->numOutputFrames + 1, ptr->timeOffset, ptr->timeStampJump);

        if( (ptr->defferedInputTimeStamp > nextExpectedTimeStamp) || (mfxU64)(abs((mfxI32)(ptr->defferedInputTimeStamp - nextExpectedTimeStamp))) < m_minDeltaTime )
        {
            out->Info.PicStruct = ptr->refPicStruct[1];

            sts = MFX_ERR_MORE_SURFACE;
        }
        else
        {
            ptr->bReadyOutput = false;

            out->Info.PicStruct = ptr->refPicStruct[0];

            sts = MFX_ERR_NONE;
        }

        out->Data.TimeStamp = ptr->expectedTimeStamp;
        ptr->numOutputFrames++;

        return sts;
    }

    //------------------------------------------------
    //           standard processing
    //------------------------------------------------
    MFX_CHECK_NULL_PTR1( in );
    mfxU64 inputTimeStamp = in->Data.TimeStamp;// pParam->inTimeStamp;

    if( false == ptr->bIsSetTimeOffset )
    {
        ptr->bIsSetTimeOffset = true;
        ptr->timeOffset = inputTimeStamp;
    }

    // calculate expected timestamp based on output framerate
    ptr->expectedTimeStamp = GetExpectedPTS(ptr->numOutputFrames, ptr->timeOffset, ptr->timeStampJump); 

    mfxU32 timeStampDifference = abs((mfxI32)(inputTimeStamp - ptr->expectedTimeStamp));

    // process irregularity
    if (m_minDeltaTime > timeStampDifference)
    {
        inputTimeStamp = ptr->expectedTimeStamp;
    }

    // made decision regarding frame rate conversion
    if ( inputTimeStamp < ptr->expectedTimeStamp )
    {
        ptr->bReadyOutput     = false;        

        ptr->refPicStruct[0] = in->Info.PicStruct;

        // skip frame
        // request new one input surface
        return MFX_ERR_MORE_DATA;
    }
    else if ( inputTimeStamp == ptr->expectedTimeStamp ) // see above (minDelta)
    {        
        ptr->bReadyOutput     = false;                

        ptr->refPicStruct[0] = in->Info.PicStruct;        
        out->Info.PicStruct = in->Info.PicStruct;
        out->Data.TimeStamp = ptr->expectedTimeStamp;

        ptr->numOutputFrames++;

        return MFX_ERR_NONE;
    }
    else // inputTimeStampParam > ptr->expectedTimeStamp
    {         
        ptr->defferedInputTimeStamp = inputTimeStamp;

        mfxU64 nextExpectedTimeStamp = GetExpectedPTS(ptr->numOutputFrames + 1, ptr->timeOffset, ptr->timeStampJump);

        if( (ptr->defferedInputTimeStamp > nextExpectedTimeStamp) || (mfxU64)(abs((mfxI32)(ptr->defferedInputTimeStamp - nextExpectedTimeStamp))) < m_minDeltaTime )
        {
            ptr->bReadyOutput = true;

            out->Info.PicStruct = ptr->refPicStruct[0];
            ptr->refPicStruct[1] = ptr->refPicStruct[0];
            ptr->refPicStruct[0] = in->Info.PicStruct;

            sts = MFX_ERR_MORE_SURFACE;
        }
        else
        {
            ptr->bReadyOutput = false;            

            out->Info.PicStruct = in->Info.PicStruct;
            ptr->refPicStruct[0] = in->Info.PicStruct;

            sts = MFX_ERR_NONE;
        }
        
        out->Data.TimeStamp = ptr->expectedTimeStamp;

        ptr->numOutputFrames++;

        return sts;
    }

} // MFXVideoVPPFrameRateConversion::CheckProduceOutput_AdvancedFRC(...)


// function is called from sync part of VPP only
mfxStatus MFXVideoVPPFrameRateConversion::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
{
    if( NULL == out )
    {
        return MFX_ERR_NULL_PTR;
    }

    if( (NULL == in) && (!IsReadyOutput(MFX_REQUEST_FROM_VPP_CHECK)) )
    {
        return MFX_ERR_MORE_DATA;
    }

    if( m_bAdvancedMode )
    {
        return CheckProduceOutput_AdvancedFRC(in, out);
    }
    else
    {
        return CheckProduceOutput_NativeFRC(in, out);
    }    

} // mfxStatus MFXVideoVPPFrameRateConversion::CheckProduceOutput(...)


bool MFXVideoVPPFrameRateConversion::IsReadyOutput( mfxRequestType requestType )
{
    if( !m_bAdvancedMode )
    {
        NativeFRCState state = m_nativeSyncState;

        if( MFX_REQUEST_FROM_VPP_PROCESS == requestType )
        {
            state = m_nativeProcessState;
        }

        return state.bReadyOutput; 
    }
    else
    {
        AdvancedFRCState state = m_advSyncState;

        if( MFX_REQUEST_FROM_VPP_PROCESS == requestType )
        {
            state = m_advProcessState;
        }

        return state.bReadyOutput; 
    }

} // bool MFXVideoVPPFrameRateConversion::IsReadyOutput( mfxRequestType requestType )


mfxStatus MFXVideoVPPFrameRateConversion::ResetNativeState( NativeFRCState* pState )
{
    pState->deltaTime = 0.0;
    pState->bReadyOutput = false;
    pState->refPicStruct = MFX_PICSTRUCT_UNKNOWN;
    memset(&(pState->defferedFrameInfo), 0, sizeof(mfxFrameInfo));

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPFrameRateConversion::ResetNativeState( NativeFRCState* pState )


mfxStatus MFXVideoVPPFrameRateConversion::ResetAdvancedState( AdvancedFRCState* pState )
{
    pState->bIsSetTimeOffset = false;
    
    pState->expectedTimeStamp = 0;
    pState->timeStampJump = 0;
    pState->timeOffset = 0;
    pState->numOutputFrames = 0;
    //pState->m_minDeltaTime = 0;

    pState->defferedInputTimeStamp = 0;
    pState->bReadyOutput = false;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPFrameRateConversion::ResetAdvancedState( AdvancedFRCState* pState )

#endif // MFX_ENABLE_HW_ONLY_VPP
#endif // MFX_ENABLE_VPP
/* EOF */
