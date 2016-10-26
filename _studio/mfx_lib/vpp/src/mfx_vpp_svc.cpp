//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include <math.h>
#include <algorithm>

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#include "mfx_enc_common.h"
#include "mfx_session.h"
#include "mfxsvc.h"

#include "mfx_vpp_utils.h"
#include "mfx_vpp_svc.h"

using namespace MfxVideoProcessing;

mfxStatus ImplementationSvc::Query(VideoCORE* core, mfxVideoParam *in, mfxVideoParam *out)
{
    return VideoVPPBase::Query( core, in, out);

} // mfxStatus ImplementationSvc::Query(VideoCORE* core, mfxVideoParam *in, mfxVideoParam *out)


mfxStatus ImplementationSvc::QueryIOSurf(VideoCORE* core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    //aya: interface SVC_VPP isn't clear right now.
    mfxVideoParam queryVideoParam = *par;
    queryVideoParam.vpp.Out = queryVideoParam.vpp.In; //aya: it is right, exclude DI processing
    queryVideoParam.vpp.Out.PicStruct = par->vpp.Out.PicStruct;// aya: it is OK for DI

    mfxStatus sts = VideoVPPBase::QueryIOSurf(core, &queryVideoParam, request);

    return sts;

} // mfxStatus ImplementationSvc::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request, const mfxU32 adapterNum)


ImplementationSvc::ImplementationSvc(VideoCORE *core)
: m_bInit( false )
, m_core( core )
, m_svcDesc()
, m_inputSurface(0)
{
} // ImplementationSvc::ImplementationSvc(VideoCORE *core)


ImplementationSvc::~ImplementationSvc()
{
    Close();

} // ImplementationSvc::~ImplementationSvc()


mfxStatus ImplementationSvc::Init(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1( par );

    if( m_bInit )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    // [1] SVC Configuration
    //mfxExtSVCSeqDesc* pHintSVC = NULL;
    mfxExtBuffer*     pHint    = NULL;

    GetFilterParam(par, MFX_EXTBUFF_SVC_SEQ_DESC, &pHint);
    MFX_CHECK_NULL_PTR1( pHint );

    m_svcDesc = *((mfxExtSVCSeqDesc*)pHint);

    // [2] initialization
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxStatus intSts = MFX_ERR_NONE;
    mfxVideoParam layerParam;

    m_declaredDidList.clear();
    m_recievedDidList.clear();

    for( mfxU32 did = 0; did < 8; did++ )
    {
        if( 0 == m_svcDesc.DependencyLayer[did].Active )
        {
            continue;
        }

        GetLayerParam(par, layerParam, did);
        VideoVPPBase * vpp = CreateAndInitVPPImpl(&layerParam, m_core, &mfxSts);
        if( 0 == vpp)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        if(MFX_ERR_NONE > mfxSts)
        {
            return mfxSts;
        }

        if (MFX_WRN_PARTIAL_ACCELERATION == mfxSts || MFX_WRN_FILTER_SKIPPED == mfxSts || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxSts)
        {
            intSts = mfxSts;
            mfxSts = MFX_ERR_NONE;
        }

        m_VPP[did].reset(vpp);

        // keep for Execute
        m_declaredDidList.push_back(did);
    }

    m_bInit = true;

    return intSts;

} // mfxStatus ImplementationSvc::Init(mfxVideoParam *par)


mfxStatus ImplementationSvc::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1( par );

    //VPP_CHECK_NOT_INITIALIZED;
    if( !m_bInit )
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    bool bWarningIncompatible = false;
    mfxStatus sts = CheckExtParam(m_core, par->ExtParam,  par->NumExtParam);
    if( MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts )
    {
        sts = MFX_ERR_NONE;
        bWarningIncompatible = true;
    }
    MFX_CHECK_STS(sts);

    mfxVideoParam layerParam;
    mfxStatus mfxSts;
    for( mfxU32 did = 0; did < 8; did++)
    {
        if( m_VPP[did].get() )
        {
            GetLayerParam(par, layerParam, did);
            mfxSts = m_VPP[did]->Reset( &layerParam );
            MFX_CHECK_STS( mfxSts );
        }
    }

    m_recievedDidList.clear();
    m_inputSurface = NULL;

    if( bWarningIncompatible )
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }
    else
    {
        return MFX_ERR_NONE;
    }

} // mfxStatus ImplementationSvc::Reset(mfxVideoParam *par)


mfxStatus ImplementationSvc::GetVideoParam(mfxVideoParam *par)
{
    if( !m_bInit )
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxVideoParam layerParam;
    for( mfxU32 did = 0; did < 8; did++)
    {
        if( m_VPP[did].get() )
        {
            GetLayerParam(par, layerParam, did);
            mfxSts = m_VPP[did]->GetVideoParam( &layerParam );

            break;
        }
    }

    return mfxSts;

} // mfxStatus VideoVPPMain::GetVideoParam(mfxVideoParam *par)


mfxStatus ImplementationSvc::GetVPPStat(mfxVPPStat *stat)
{
    if( !m_bInit )
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus mfxSts = MFX_ERR_NONE;
//    mfxVideoParam layerParam;
    for( mfxU32 did = 0; did < 8; did++)
    {
        if( m_VPP[did].get() )
        {
            //GetLayerParam(par, layerParam, did);
            mfxSts = m_VPP[did]->GetVPPStat( stat );

            break;
        }
    }

    return mfxSts;

} // mfxStatus ImplementationSvc::GetVPPStat(mfxVPPStat *stat)


mfxTaskThreadingPolicy ImplementationSvc::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_INTRA;

} // mfxTaskThreadingPolicy ImplementationSvc::GetThreadingPolicy(void)


mfxStatus ImplementationSvc::VppFrameCheck(
    mfxFrameSurface1 *in,
    mfxFrameSurface1 *out,
    mfxExtVppAuxData *aux,
    MFX_ENTRY_POINT pEntryPoint[],
    mfxU32 &numEntryPoints)
{
    MFX_CHECK_NULL_PTR1( out );

    mfxStatus mfxSts;

    if( !m_bInit )
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    mfxU32 did = out->Info.FrameId.DependencyId;

    std::vector<mfxU32>::iterator itDid = find(m_declaredDidList.begin(), m_declaredDidList.end(), did);
    if( m_declaredDidList.end() == itDid )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;//MFX_ERR_INVALID_VIDEO_PARAM;
    }

    itDid = find(m_recievedDidList.begin(), m_recievedDidList.end(), did);
    if( m_recievedDidList.end() != itDid )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;//MFX_ERR_INVALID_VIDEO_PARAM;
    }
    m_recievedDidList.push_back(did);

    if( 1 == m_recievedDidList.size() )
    {
        m_inputSurface = in;
    }
    else
    {
        if( m_inputSurface != in )
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }


    mfxSts = m_VPP[ did ]->VppFrameCheck( in, out, aux, pEntryPoint, numEntryPoints );

    // advanced processing (delay or multi outputs)
    if( (MFX_ERR_MORE_DATA == mfxSts || 
         MFX_ERR_MORE_SURFACE == mfxSts ||
         (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK == mfxSts) &&
         (m_recievedDidList.size() == m_declaredDidList.size()) )
    {
        m_recievedDidList.clear();
        m_inputSurface = NULL;

        return mfxSts;
    }

    // simple processing (no delay, no multi outputs)
    if( MFX_ERR_NONE == mfxSts )
    {
        if( m_recievedDidList.size() < m_declaredDidList.size() )
        {
            mfxSts = MFX_ERR_MORE_SURFACE;
        }
        else
        {
            m_recievedDidList.clear();
            m_inputSurface = NULL;
        }
    }

    return mfxSts;

} // mfxStatus ImplementationSvc::VppFrameCheck(...)

// depreciated
mfxStatus ImplementationSvc::RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux)
{
    mfxU32 did = out->Info.FrameId.DependencyId;
    mfxStatus mfxSts = m_VPP[ did ]->RunFrameVPP( in, out, aux);

    return mfxSts;

} // mfxStatus ImplementationSvc::RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux)


mfxStatus ImplementationSvc::Close(void)
{
 //VPP_CHECK_NOT_INITIALIZED;
    if( !m_bInit )
    {
        return MFX_ERR_NONE;
    }

    for(mfxU32 did = 0; did < 8; did++)
    {
        m_VPP[did].reset();
    }

    m_bInit = false;

    m_recievedDidList.clear();
    m_declaredDidList.clear();
    m_inputSurface = NULL;

    return MFX_ERR_NONE;

} // mfxStatus ImplementationSvc::Close( void )


void ImplementationSvc::GetLayerParam(
    mfxVideoParam *par,
    mfxVideoParam & layerParam,
    mfxU32 did)
{
    layerParam  = *par;
    layerParam.vpp.Out.Width  = m_svcDesc.DependencyLayer[did].Width;
    layerParam.vpp.Out.Height = m_svcDesc.DependencyLayer[did].Height;

    layerParam.vpp.Out.CropX = m_svcDesc.DependencyLayer[did].CropX;
    layerParam.vpp.Out.CropY = m_svcDesc.DependencyLayer[did].CropY;
    layerParam.vpp.Out.CropW = m_svcDesc.DependencyLayer[did].CropW;
    layerParam.vpp.Out.CropH = m_svcDesc.DependencyLayer[did].CropH;

    return;
}

#endif // MFX_ENABLE_VPP
/* EOF */
