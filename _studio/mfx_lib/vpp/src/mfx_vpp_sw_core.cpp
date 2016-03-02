/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//
//
//          Common Video Pre\Post Processing
//
*/

#include "math.h"
#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#include "mfx_enc_common.h"
#include "mfx_session.h"

#include "mfx_vpp_hw.h"
#include "libmfx_core.h"
#include "libmfx_core_factory.h"
#include "libmfx_core_interface.h"
#include "mfxpcp.h"

#include "mfx_vpp_utils.h"
#include "mfx_vpp_service.h"
#include "mfx_vpp_sw.h"

/* IPP */
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"


using namespace MfxHwVideoProcessing;


/* ******************************************************************** */
/*                           useful macros                              */
/* ******************************************************************** */

#ifndef VPP_CHECK_STS_SAFE
#define VPP_CHECK_STS_SAFE(sts, in, out)               \
{                                                      \
    if (sts != MFX_ERR_NONE && in)                       \
{                                                    \
    m_core->DecreaseReference( &(in->Data) );          \
}                                                    \
    if (sts != MFX_ERR_NONE && out)                      \
{                                                    \
    m_core->DecreaseReference( &(out->Data) );         \
}                                                    \
    MFX_CHECK_STS( sts );                                \
}
#endif


#define VPP_UPDATE_STAT( sts, stat )                      \
{                                                         \
    if( MFX_ERR_NONE == sts ) stat.NumFrame++;              \
    if( MFX_ERR_NULL_PTR == sts ) stat.NumCachedFrame++;\
}

#define VPP_UNLOCK_SURFACE(sts, surface)                  \
{                                                         \
    if( MFX_ERR_NONE == sts )  m_core->DecreaseReference( &surface->Data );\
}


void ConvertCaps2ListDoUse(MfxHwVideoProcessing::mfxVppCaps& caps, std::vector<mfxU32>& list);


/* ******************************************************************** */
/*      implementation of VPP Pipeline: interface methods               */
/* ******************************************************************** */

VideoVPPSW::VideoVPPSW(VideoCORE *core, mfxStatus* sts )
{
    mfxU32 filterIndex  = 0;
    mfxU32 connectIndex = 0;

    for( filterIndex = 0; filterIndex < MAX_NUM_VPP_FILTERS; filterIndex++ )
    {
        m_ppFilters[filterIndex]        = NULL;
        //m_pipelineList[filterIndex] = NULL;
    }

    for( connectIndex = 0; connectIndex < MAX_NUM_CONNECTIONS; connectIndex++ )
    {
        m_connectionFramesPool[connectIndex].Zero();
    }

    /* surfaces for Fast Copy using */
    m_internalSystemFramesPool[VPP_IN].Zero();
    m_bDoFastCopyFlag[VPP_IN] = false;

    m_internalSystemFramesPool[VPP_OUT].Zero();
    m_bDoFastCopyFlag[VPP_OUT] = false;

    /* opaque processing */
    m_bOpaqMode[VPP_IN]  = false;
    m_bOpaqMode[VPP_OUT] = false;

    m_requestOpaq[VPP_IN].NumFrameMin = m_requestOpaq[VPP_IN].NumFrameSuggested = 0;
    m_requestOpaq[VPP_IN].Type = 0;

    m_requestOpaq[VPP_OUT].NumFrameMin = m_requestOpaq[VPP_OUT].NumFrameSuggested = 0;
    m_requestOpaq[VPP_OUT].Type = 0;

    /* common */
    m_core         = core;
    //m_numUsedFilters = NULL;
    m_memIdWorkBuf   = NULL;

    m_stat.NumCachedFrame = 0;
    m_stat.NumFrame       = 0;

    m_bDynamicDeinterlace = false;

    VPP_CLEAN;

    *sts = MFX_ERR_NONE;

    m_pHWVPP.reset(0);

} // VideoVPPSW::VideoVPPSW(VideoCORE *core, mfxStatus* sts ) : VideoVPP()

VideoVPPSW::~VideoVPPSW()
{
    Close();

} // VideoVPPSW::~VideoVPPSW()

mfxStatus VideoVPPSW::Close(void)
{

    VPP_CHECK_NOT_INITIALIZED;

    DestroyPipeline();

    DestroyConnectionFramesPool();

    DestroyInternalSystemFramesPool();

    DestroyWorkBuffer();

    m_stat.NumCachedFrame = 0;
    m_stat.NumFrame       = 0;

    m_bDynamicDeinterlace = false;

    /* opaque processing */

    if( m_bOpaqMode[VPP_IN] )
    {
        m_requestOpaq[VPP_IN].NumFrameMin = m_requestOpaq[VPP_IN].NumFrameSuggested = 0;
        m_requestOpaq[VPP_IN].Type = 0;
    }

    if( m_bOpaqMode[VPP_OUT] )
    {
        m_requestOpaq[VPP_OUT].NumFrameMin = m_requestOpaq[VPP_OUT].NumFrameSuggested = 0;
        m_requestOpaq[VPP_OUT].Type = 0;
    }

    m_bOpaqMode[VPP_IN] = m_bOpaqMode[VPP_OUT] = false;

    //m_numUsedFilters      = 0;
    m_pipelineList.resize(0);

    if( m_pHWVPP.get() )
    {
        m_pHWVPP.reset(0);
    }

    VPP_CLEAN;

    return MFX_ERR_NONE;// in according with spec

} // mfxStatus VideoVPPSW::Close(void)

mfxStatus VideoVPPSW::Init(mfxVideoParam *par)
{
    mfxStatus sts  = MFX_ERR_INVALID_VIDEO_PARAM;
    mfxStatus sts_wrn = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1( par );

    VPP_CHECK_MULTIPLE_INIT;

    /* step [0]: checking */
    sts = CheckIOPattern( par );
    MFX_CHECK_STS(sts);

    /* SW doesn't support of protected processing */
    sts = CheckProtectedMode( par->Protected );
    MFX_CHECK_STS(sts);

    sts = CheckFrameInfo( &(par->vpp.In), VPP_IN );
    MFX_CHECK_STS( sts );

    sts = CheckFrameInfo( &(par->vpp.Out), VPP_OUT );
    MFX_CHECK_STS(sts);

    PicStructMode picStructMode = GetPicStructMode(par->vpp.In.PicStruct, par->vpp.Out.PicStruct);
    m_bDynamicDeinterlace = (DYNAMIC_DI_PICSTRUCT_MODE == picStructMode) ? true : false;

    sts = CheckExtParam(m_core, par->ExtParam,  par->NumExtParam);
    if( MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts || MFX_WRN_FILTER_SKIPPED == sts)
    {
        sts_wrn = sts;
        sts = MFX_ERR_NONE;
    }
    MFX_CHECK_STS(sts);

    /* step [1]: building stage of VPP pipeline */
    sts = GetPipelineList( par, m_pipelineList, true);
    MFX_CHECK_STS(sts);


    // opaque configuration rules:
    // (1) in case of OPAQ request VPP should ignore IOPattern and use extBuffer native memory type
    // (2) VPP_IN abd VPP_OUT should be checked independently of one another
    sts = CheckOpaqMode( par, m_bOpaqMode );
    MFX_CHECK_STS( sts );

    if( m_bOpaqMode[VPP_IN] || m_bOpaqMode[VPP_OUT] )
    {
        sts = GetOpaqRequest( par, m_bOpaqMode, m_requestOpaq);
        MFX_CHECK_STS( sts );

        // VPP controls OPAQUE request.
        // will be combined with SW::CreatePipeline() to prevent multu run of QueryIOSurf()
        {
            mfxFrameAllocRequest  cntrlRequest[2];
            sts = QueryIOSurf(m_core, par, cntrlRequest);
            VPP_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
            MFX_CHECK_STS( sts );

            if( m_bOpaqMode[VPP_IN] &&
                (m_requestOpaq[VPP_IN].NumFrameMin < cntrlRequest[VPP_IN].NumFrameMin ||
                m_requestOpaq[VPP_IN].NumFrameSuggested < cntrlRequest[VPP_IN].NumFrameSuggested) )
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            if( m_bOpaqMode[VPP_OUT] &&
                (m_requestOpaq[VPP_OUT].NumFrameMin < cntrlRequest[VPP_OUT].NumFrameMin ||
                m_requestOpaq[VPP_OUT].NumFrameSuggested < cntrlRequest[VPP_OUT].NumFrameSuggested) )
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }
        }
    }

    // try to use HW VPP
    if( MFX_PLATFORM_HARDWARE == m_core->GetPlatformType() )
    {
        CommonCORE* pCommonCore = NULL;

        bool isFieldProcessing = IsFilterFound(&m_pipelineList[0], (mfxU32)m_pipelineList.size(), MFX_EXTBUFF_VPP_FIELD_PROCESSING);
        
        pCommonCore = QueryCoreInterface<CommonCORE>(m_core, isFieldProcessing ? MFXICORECM_GUID : MFXIVideoCORE_GUID);
        MFX_CHECK(pCommonCore, MFX_ERR_UNDEFINED_BEHAVIOR);

        // trying HW VPP
        if (!((pCommonCore)->m_ExtOptions & MFX_EXTOPTION_VPP_SW))
        {
            VideoVPPHW::IOMode mode = VideoVPPHW::GetIOMode(par, m_requestOpaq);

            m_pHWVPP.reset(new VideoVPPHW(mode, m_core));

            if(isFieldProcessing) {
                m_pHWVPP.get()->SetCmDevice(pCommonCore);
            }
            sts = m_pHWVPP.get()->Init(par); // OK or ERR only
            if (MFX_WRN_FILTER_SKIPPED == sts)
            {
                // do not break execution, skip filter later
                sts = MFX_ERR_NONE;
            }
            if (MFX_ERR_NONE != sts)
            {
                m_pHWVPP.reset(0);
            }
            if (MFX_WRN_PARTIAL_ACCELERATION == sts)
            {
                // do not break execution, fall back to SW
                sts = MFX_ERR_NONE;
            }
            MFX_CHECK_STS( sts );

            //m_pHWVPP.get()->SetCmDevice(pCommonCore);
        }
    }

#if !defined(MFX_ENABLE_HW_ONLY_VPP)
    if(m_pHWVPP.get() == 0) // MARKER: SW only
    {
        /* step [2]: creation of filters */
        sts = CreatePipeline( &(par->vpp.In), &(par->vpp.Out) );
        MFX_CHECK_STS( sts );

        /* filters can require work buffers */
        sts = CreateWorkBuffer();
        MFX_CHECK_STS( sts )

        sts = CreateConnectionFramesPool( );
        MFX_CHECK_STS( sts );

        // important!!! this function uses OPAQUE information (m_bOpaqMode/m_requestOpaq)
        sts = CreateInternalSystemFramesPool( par );
        MFX_CHECK_STS( sts );
    }
#endif
    /* save init params to prevent core crash */
    m_errPrtctState.In  = par->vpp.In;
    m_errPrtctState.Out = par->vpp.Out;
    m_errPrtctState.IOPattern  = par->IOPattern;
    m_errPrtctState.AsyncDepth = par->AsyncDepth;

    sts = GetCompositionEnabledStatus(par);
    if (sts == MFX_ERR_NONE)
        m_errPrtctState.isCompositionModeEnabled = true;
    else
        m_errPrtctState.isCompositionModeEnabled = false;

    m_InitState = m_errPrtctState; // Save params on init
    /* to reset status */
    sts = MFX_ERR_NONE;


    m_stat.NumCachedFrame = 0;
    m_stat.NumFrame       = 0;

    VPP_INIT_SUCCESSFUL;

    /* here, Reset == SetParam */
    mfxStatus stsReset = sts = Reset( par );
    if(MFX_WRN_FILTER_SKIPPED == sts || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts)
    {
        sts = MFX_ERR_NONE;
    }
    MFX_CHECK_STS( sts );

    if( MFX_PLATFORM_HARDWARE == m_core->GetPlatformType() )
    {

        if ( 0 == m_pHWVPP.get() && IS_PROTECTION_ANY(par->Protected))
        {
            return MFX_ERR_UNSUPPORTED;
        }
        else if ((MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY) != par->IOPattern &&
            IS_PROTECTION_ANY(par->Protected))
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else if( 0 == m_pHWVPP.get() )
        {
#if defined (MFX_ENABLE_HW_ONLY_VPP)
            return MFX_ERR_UNSUPPORTED;
#else
            return MFX_WRN_PARTIAL_ACCELERATION;
#endif
        }
        else if( MFX_ERR_NONE != sts_wrn )
        {
            return sts_wrn;
        }
    }
    else
    {

#if defined (MFX_ENABLE_HW_ONLY_VPP)
        return MFX_ERR_UNSUPPORTED;
#else
        if( MFX_ERR_NONE != sts_wrn )
        {
            return sts_wrn;
        }
#endif
    }

    bool bCorrectionEnable = false;
    sts = CheckPlatformLimitations(m_core, *par, bCorrectionEnable);

    if (MFX_ERR_UNSUPPORTED == sts)
    {
        sts = MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return (MFX_ERR_NONE == sts) ? stsReset : sts;

} // mfxStatus VideoVPPSW::Init(mfxVideoParam *par)


mfxStatus VideoVPPSW::RunFrameVPP(mfxFrameSurface1* in, mfxFrameSurface1* out, mfxExtVppAuxData *aux)
{
    mfxU32 filterIndex = 0;
    mfxStatus sts = MFX_ERR_NONE;
    mfxStatus processingSts = MFX_ERR_NONE;

    mfxFrameSurface1* pInSurf = in;
    mfxFrameSurface1* pOutSurf= out;

    mfxFrameSurface1* pipelineInSurf;
    mfxFrameSurface1* pipelineOutSurf;

    bool bLockedInput  = false;
    bool bLockedOutput = false;

    FilterVPP::InternalParam  internalParam;
    // VPP ignores input surface if there is ready output from any filter
    bool bUseInput    = true;
    VPP_CHECK_NOT_INITIALIZED;

    /* *************************************** */
    /*              pre-work                   */
    /* *************************************** */

    if (!m_bDoFastCopyFlag[VPP_IN])
    {
        sts = m_externalFramesPool[VPP_IN].PreProcessSync();
        MFX_CHECK_STS( sts );
    }
    else
    {
        sts = m_internalSystemFramesPool[VPP_IN].Lock();
        MFX_CHECK_STS( sts );
    }

    if (!m_bDoFastCopyFlag[VPP_OUT])
    {
        sts = m_externalFramesPool[VPP_OUT].PreProcessSync();
        MFX_CHECK_STS( sts );
    }


    bLockedInput = false;
    pInSurf      = NULL;
    if( IsReadyOutput(MFX_REQUEST_FROM_VPP_PROCESS) || (NULL == in) )
    {
        bUseInput = false;
    }

    if( bUseInput )
    {
        sts = PreProcessOfInputSurface(in, &pInSurf);
        MFX_CHECK_STS( sts );
    }

    bLockedOutput = false;
    sts = PreProcessOfOutputSurface(out, &pOutSurf);
    MFX_CHECK_STS( sts );


    /* VPP ignores crop info during init stage. so, current frame gives this information.
    in according with spec, crop info can changed by every frame */
    sts = SetCrop(pInSurf, pOutSurf);
    MFX_CHECK_STS( sts );

    // lock internal surfaces
    mfxU32 connectIndex;
    for( connectIndex = 0; connectIndex < GetNumConnections(); connectIndex++ )
    {
        sts = m_connectionFramesPool[connectIndex].Lock();
        MFX_CHECK_STS( sts );
    }

    /* *************************************** */
    /*              processing                 */
    /* *************************************** */

    // input picstruct must be stored in Check(). here inPicStruct should be ignored
    internalParam.aux = aux;
    internalParam.outPicStruct = (in) ? in->Info.PicStruct : (mfxU16)MFX_PICSTRUCT_UNKNOWN;
    internalParam.outTimeStamp = (in) ? in->Data.TimeStamp : (mfxU64)MFX_TIMESTAMP_UNKNOWN;

    // in case of advanced algorithms VPP can produce output wo input.
    mfxU32 filterIndexStart = GetFilterIndexStart( MFX_REQUEST_FROM_VPP_PROCESS );
    pipelineOutSurf = (filterIndexStart > 0) ? NULL : pInSurf;

    for( filterIndex = filterIndexStart; filterIndex < GetNumUsedFilters(); filterIndex++ )
    {
        pipelineInSurf = pipelineOutSurf;
        internalParam.inPicStruct = internalParam.outPicStruct;
        internalParam.inTimeStamp = internalParam.outTimeStamp;

        if( filterIndex >= GetNumConnections() )
        {
            pipelineOutSurf = pOutSurf;
        }
        else
        {
            sts = m_connectionFramesPool[filterIndex].GetFreeSurface( &pipelineOutSurf );
            MFX_CHECK_STS( sts );
        }

        processingSts = m_ppFilters[filterIndex]->RunFrameVPP(pipelineInSurf, pipelineOutSurf, &internalParam);

        /* downsampleFRC breaks pipeline in case of MFX_ERR_MORE_DATA
        it is reason to stop processing given moment
        */
        if( (MFX_ERR_MORE_DATA == processingSts) && (MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION == m_pipelineList[filterIndex]) )
        {
            break;
        }

        if( MFX_ERR_MORE_DATA == processingSts )
        {
            pipelineOutSurf = NULL;
        }
        else
        {// real status is returned from sync pat of VPP
            VPP_IGNORE_MFX_STS(processingSts, MFX_ERR_MORE_SURFACE);

            if ( processingSts == MFX_ERR_UNDEFINED_BEHAVIOR)
                MFX_CHECK_STS( processingSts );
        }
    }// processing finished


    /* *************************************** */
    /*              post-work                  */
    /* *************************************** */
    if( MFX_ERR_NONE == processingSts )
    {
        sts = SetBackGroundColor( pOutSurf );
        MFX_CHECK_STS( sts );
    }

    if (!m_bDoFastCopyFlag[VPP_IN])
    {
        sts = m_externalFramesPool[VPP_IN].PostProcessSync();
        MFX_CHECK_STS( sts );
    }
    else
    {
        sts = m_internalSystemFramesPool[VPP_IN].Lock();
        MFX_CHECK_STS( sts );
    }

    if (!m_bDoFastCopyFlag[VPP_OUT])
    {
        sts = m_externalFramesPool[VPP_OUT].PostProcessSync();
        MFX_CHECK_STS( sts );
    }

    if( bUseInput )
    {
        if (!m_bDoFastCopyFlag[VPP_IN])
        {
            /* for each non zero input frame */
            sts = m_core->DecreaseReference( &in->Data );
            MFX_CHECK_STS( sts );
        }

        /* for each non zero input frame */
        sts = m_core->DecreaseReference( &pInSurf->Data );
        MFX_CHECK_STS( sts );
    }

    sts = PostProcessOfOutputSurface(out, pOutSurf, processingSts);
    MFX_CHECK_STS( sts );

    sts = m_core->DecreaseReference( &pOutSurf->Data );
    MFX_CHECK_STS( sts );

    // unlock internal surfaces
    for( connectIndex = 0; connectIndex < GetNumConnections(); connectIndex++ )
    {
        sts = m_connectionFramesPool[connectIndex].Unlock();
        MFX_CHECK_STS( sts );
    }

    /* *********************************************************** */
    /*                      STOP                                   */
    /* *********************************************************** */

    /* once per RunFrameVPP */
    if( out && MFX_ERR_NONE == processingSts )
    {
        sts = m_core->DecreaseReference( &(out->Data) );
        MFX_CHECK_STS( sts );
    }

    if( !m_errPrtctState.isFirstFrameProcessed )
    {
        m_errPrtctState.isFirstFrameProcessed = true;
    }

    return sts;

} // mfxStatus VideoVPPSW::RunFrameVPP(


mfxStatus VideoVPPSW::VppFrameCheck(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux,
                                    MFX_ENTRY_POINT pEntryPoints[], mfxU32 &numEntryPoints)
{

    //printf("\nVideoVPPSW::VppFrameCheck()\n"); fflush(stdout);

    mfxStatus sts = MFX_ERR_NONE;
    mfxStatus stsReadinessPipeline = MFX_ERR_NONE; stsReadinessPipeline;

    /* [IN] */
    // it is end of stream procedure if(NULL == in)

    if( NULL == out )
    {
        return MFX_ERR_NULL_PTR;
    }

    if (out->Data.Locked)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    // need for SW due to algorithm processing
    mfxU16  realOutPicStruct = out->Info.PicStruct; realOutPicStruct;

    /* *************************************** */
    /*              check info                 */
    /* *************************************** */
    if (in)
    {
        sts = CheckInputPicStruct( in->Info.PicStruct );
        MFX_CHECK_STS(sts);

        /* we have special case for composition:
         * if composition enabled sub stream's picture (WxH)
         * can be less than primary stream (WxH)
         * So, do check frame info only if composition is not enabled */
        if (m_errPrtctState.isCompositionModeEnabled == false)
        {
            sts = CompareFrameInfo( &(in->Info), &(m_errPrtctState.In));
            MFX_CHECK_STS(sts);
        }

        sts = CheckCropParam( &(in->Info) );
        MFX_CHECK_STS( sts );
    }

    sts = CompareFrameInfo( &(out->Info), &(m_errPrtctState.Out));
    MFX_CHECK_STS(sts);

    sts = CheckCropParam( &(out->Info) );
    MFX_CHECK_STS( sts );

    if ( m_pHWVPP.get() )
    {
        mfxStatus internalSts = MFX_ERR_NONE;

        internalSts = m_pHWVPP.get()->VppFrameCheck( in, out, aux, pEntryPoints, numEntryPoints);

        bool isInverseTelecinedEnabled = false;
        isInverseTelecinedEnabled = IsFilterFound(&m_pipelineList[0], (mfxU32)m_pipelineList.size(), MFX_EXTBUFF_VPP_ITC);

        if (MFX_ERR_MORE_DATA == internalSts && true == isInverseTelecinedEnabled)
        {
            //internalSts = (mfxStatus) MFX_ERR_MORE_DATA_RUN_TASK;
        }

        if( out && (MFX_ERR_NONE == internalSts || MFX_ERR_MORE_SURFACE == internalSts) )
        {
            sts = PassThrough( NULL != in ? &(in->Info) : NULL, &(out->Info));
            //MFX_CHECK_STS( sts );
        }

        return (MFX_ERR_NONE == internalSts) ? sts : internalSts;
    }
#if !defined (MFX_ENABLE_HW_ONLY_VPP)
    /* *************************************** */
    /* scan filters to find Ready Output       */
    /* if the out found VPP IGNORES input frame*/
    /* *************************************** */
    bool bIgnoreInput = IsReadyOutput(MFX_REQUEST_FROM_VPP_CHECK);

    /* *************************************** */
    /* Check Readiness Pipeline To Produce Out */
    /* *************************************** */

    stsReadinessPipeline = CheckProduceOutput(in, out);

    /* *************************************** */
    /*              hold  surfaces             */
    /* *************************************** */
    if( in && !bIgnoreInput && (MFX_ERR_NONE == stsReadinessPipeline ||
        static_cast<int>(MFX_ERR_MORE_DATA_RUN_TASK) == static_cast<int>(stsReadinessPipeline) ||
        MFX_ERR_MORE_SURFACE == stsReadinessPipeline) )
    {
        sts = m_core->IncreaseReference( &(in->Data) );
        MFX_CHECK_STS( sts );
    }

    mfxStatus stsPicStruct = MFX_ERR_NONE;
    if( out && (MFX_ERR_NONE == stsReadinessPipeline || MFX_ERR_MORE_SURFACE == stsReadinessPipeline) )
    {
        sts = m_core->IncreaseReference( &(out->Data) );
        MFX_CHECK_STS( sts );

        bool bHWLib = false;
        stsPicStruct = PassThrough( (NULL != in) ? &(in->Info) : NULL, &(out->Info), realOutPicStruct, bHWLib);
        //MFX_CHECK_STS( sts );
    }

    /* initialization for multi threading */
    if (MFX_ERR_NONE == stsReadinessPipeline ||
        static_cast<int>(MFX_ERR_MORE_DATA_RUN_TASK) == static_cast<int>(stsReadinessPipeline) ||
        MFX_ERR_MORE_SURFACE == stsReadinessPipeline)
    {
        AsyncParams *pAsyncParams = new AsyncParams;
        pAsyncParams->surf_in  = in;
        pAsyncParams->surf_out = out;
        pAsyncParams->aux      = aux;
        pAsyncParams->inputPicStruct = (in) ? in->Info.PicStruct : (mfxU16)MFX_PICSTRUCT_UNKNOWN;
        pAsyncParams->inputTimeStamp = (in) ? in->Data.TimeStamp : (mfxU64)MFX_TIMESTAMP_UNKNOWN;

        pEntryPoints[0].pRoutine = &RunFrameVPPRoutine;
        //pEntryPoint->pRoutine = NULL;
        pEntryPoints[0].pCompleteProc = &CompleteFrameVPPRoutine;
        pEntryPoints[0].pState = this;
        pEntryPoints[0].requiredNumThreads = 1;
        //pEntryPoints[0].requiredNumThreads = m_core->GetNumWorkingThreads();
        pEntryPoints[0].pParam = pAsyncParams;

        numEntryPoints = 1;
    }

    return (MFX_ERR_NONE == stsReadinessPipeline) ? stsPicStruct : stsReadinessPipeline;
#else
    return sts;
#endif // !MFX_ENABLE_HW_ONLY_VPP
} // mfxStatus VideoVPPSW::VppFrameCheck(...)


mfxStatus VideoVPPSW::QueryIOSurf(VideoCORE* core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxStatus mfxSts;
    core;

    MFX_CHECK_NULL_PTR2(par, request);

    mfxSts = CheckFrameInfo( &(par->vpp.In), VPP_IN);
    MFX_CHECK_STS( mfxSts );

    mfxSts = CheckFrameInfo( &(par->vpp.Out), VPP_OUT);
    MFX_CHECK_STS( mfxSts );

    // make sense?
    //mfxSts = CheckExtParam(par->ExtParam,  par->NumExtParam);
    //if( MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxSts )
    //{
    //    mfxSts = MFX_ERR_NONE;
    //    //bWarningIncompatible = true;
    //}
    //MFX_CHECK_STS(mfxSts);

    //PicStructMode m_picStructSupport = GetTypeOfInitPicStructSupport(par->vpp.In.PicStruct, par->vpp.Out.PicStruct);

    // default settings
    // VPP_IN
    request[VPP_IN].Info = par->vpp.In;
    request[VPP_IN].NumFrameMin = 1;
    request[VPP_IN].NumFrameSuggested = 1;

    //VPP_OUT
    request[VPP_OUT].Info = par->vpp.Out;
    request[VPP_OUT].NumFrameMin = 1;
    request[VPP_OUT].NumFrameSuggested = 1;

    /* correction */
    std::vector<mfxU32> pipelineList;
    //mfxU32 lenList;

    mfxSts = GetPipelineList( par, pipelineList, true );
    MFX_CHECK_STS( mfxSts );


    mfxU16 framesCountMin[2];
    mfxU16 framesCountSuggested[2];
    mfxSts = GetExternalFramesCount(par, &pipelineList[0], (mfxU32)pipelineList.size(), framesCountMin, framesCountSuggested);
    MFX_CHECK_STS( mfxSts );

    request[VPP_IN].NumFrameMin  = framesCountMin[VPP_IN];
    request[VPP_OUT].NumFrameMin = framesCountMin[VPP_OUT];

    request[VPP_IN].NumFrameSuggested  = framesCountSuggested[VPP_IN];
    request[VPP_OUT].NumFrameSuggested = framesCountSuggested[VPP_OUT];

    if( MFX_PLATFORM_HARDWARE == core->GetPlatformType() )
    {
        mfxFrameAllocRequest hwRequest[2];
        mfxSts = VideoVPPHW::QueryIOSurf(VideoVPPHW::ALL, core, par, hwRequest);

        bool bSWLib = (mfxSts == MFX_ERR_NONE) ? false : true;
        if( !bSWLib )
        {
            // suggested
            request[VPP_IN].NumFrameSuggested = IPP_MAX(request[VPP_IN].NumFrameSuggested, hwRequest[VPP_IN].NumFrameSuggested);
            request[VPP_OUT].NumFrameSuggested = IPP_MAX(request[VPP_OUT].NumFrameSuggested, hwRequest[VPP_OUT].NumFrameSuggested);

            // min
            request[VPP_IN].NumFrameMin  = IPP_MAX(request[VPP_IN].NumFrameMin, hwRequest[VPP_IN].NumFrameMin);
            request[VPP_OUT].NumFrameMin = IPP_MAX(request[VPP_OUT].NumFrameMin, hwRequest[VPP_OUT].NumFrameMin);
        }

        mfxU16 vppAsyncDepth = (0 == par->AsyncDepth) ? MFX_AUTO_ASYNC_DEPTH_VALUE : par->AsyncDepth;

        {
            // suggested
            request[VPP_IN].NumFrameSuggested  += (vppAsyncDepth - 1);
            request[VPP_OUT].NumFrameSuggested += (vppAsyncDepth - 1);

            // min
            request[VPP_IN].NumFrameMin  += (vppAsyncDepth - 1);
            request[VPP_OUT].NumFrameMin += (vppAsyncDepth - 1);
        }

        mfxSts = CheckIOPattern_AndSetIOMemTypes(par->IOPattern, &(request[VPP_IN].Type), &(request[VPP_OUT].Type), bSWLib);
        MFX_CHECK_STS(mfxSts);
#if defined (MFX_ENABLE_HW_ONLY_VPP)
        return (bSWLib)? MFX_ERR_UNSUPPORTED : MFX_ERR_NONE;
    }
#else
        return (bSWLib)? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
    }
    else
    {
        // since MSDK_3.0 asyncDepth is mandatory
        mfxU16 vppAsyncDepth = (0 == par->AsyncDepth) ? MFX_AUTO_ASYNC_DEPTH_VALUE : par->AsyncDepth;
        //vppAsyncDepth = IPP_MIN( MFX_MAX_ASYNC_DEPTH_VALUE, vppAsyncDepth);
        {
            // suggested
            request[VPP_IN].NumFrameSuggested  += (vppAsyncDepth - 1);
            request[VPP_OUT].NumFrameSuggested += (vppAsyncDepth - 1);

            // min
            request[VPP_IN].NumFrameMin  += (vppAsyncDepth - 1);
            request[VPP_OUT].NumFrameMin += (vppAsyncDepth - 1);
        }

        mfxSts = CheckIOPattern_AndSetIOMemTypes(par->IOPattern, &(request[VPP_IN].Type), &(request[VPP_OUT].Type));
        MFX_CHECK_STS( mfxSts );
    }
#endif
    return MFX_ERR_NONE;

} // mfxStatus VideoVPPSW::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request, const mfxU32 adapterNum)

mfxStatus VideoVPPSW::GetVPPStat(mfxVPPStat *stat)
{
    MFX_CHECK_NULL_PTR1(stat);

    VPP_CHECK_NOT_INITIALIZED;

    if( 0 == m_pipelineList.size() ) return MFX_ERR_NOT_INITIALIZED;

    stat->NumCachedFrame = m_stat.NumCachedFrame;
    stat->NumFrame       = m_stat.NumFrame;

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPSW::GetVPPStat(mfxVPPStat *stat)

mfxStatus VideoVPPSW::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1( par )

    par->vpp.In  = m_errPrtctState.In;
    par->vpp.Out = m_errPrtctState.Out;

    par->Protected  = 0;// AYA: should be fixed for HW_VPP
    par->IOPattern  = m_errPrtctState.IOPattern;
    par->AsyncDepth = m_errPrtctState.AsyncDepth;

    if( NULL == par->ExtParam || 0 == par->NumExtParam)
    {
        return MFX_ERR_NONE;
    }

    for( mfxU32 i = 0; i < par->NumExtParam; i++ )
    {
        if( MFX_EXTBUFF_VPP_DOUSE == par->ExtParam[i]->BufferId )
        {
            mfxExtVPPDoUse* pVPPHint = (mfxExtVPPDoUse*)(par->ExtParam[i]);
            mfxU32 numUsedFilters = 0;

            for( mfxU32 filterIndex = 0; filterIndex < GetNumUsedFilters(); filterIndex++ )
            {
                switch ( m_pipelineList[filterIndex] )
                {
                    case MFX_EXTBUFF_VPP_CSC:
                    case MFX_EXTBUFF_VPP_RESIZE:
                    case MFX_EXTBUFF_VPP_ITC:
                    case MFX_EXTBUFF_VPP_CSC_OUT_RGB4:
                    case MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10:
                    {
                        continue;
                    }
                    case MFX_EXTBUFF_VPP_DENOISE:
                    case MFX_EXTBUFF_VPP_SCENE_ANALYSIS:
                    case MFX_EXTBUFF_VPP_PROCAMP:
                    case MFX_EXTBUFF_VPP_DETAIL:
                    case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
                    case MFX_EXTBUFF_VPP_IMAGE_STABILIZATION:
                    case MFX_EXTBUFF_VPP_COMPOSITE:
                    case MFX_EXTBUFF_VPP_FIELD_PROCESSING:
                    case MFX_EXTBUFF_VPP_DEINTERLACING:
                    case MFX_EXTBUFF_VPP_DI:
                    case MFX_EXTBUFF_VPP_DI_30i60p:
                    case MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO:
                    case MFX_EXTBUFF_VPP_MIRRORING:
                    {
                        if(numUsedFilters + 1 > pVPPHint->NumAlg)
                            return MFX_ERR_UNDEFINED_BEHAVIOR;

                        pVPPHint->AlgList[numUsedFilters] = m_pipelineList[filterIndex];
                        numUsedFilters++;
                        break;
                    }
                    default:
                        return MFX_ERR_UNDEFINED_BEHAVIOR;
                }

            }
        }

        // Todo: filling of other extbuffers if required
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPSW::GetVideoParam(mfxVideoParam *par)


mfxStatus VideoVPPSW::QueryCaps(VideoCORE * core, MfxHwVideoProcessing::mfxVppCaps& caps)
{
    mfxStatus sts = MFX_ERR_NONE;

    if(MFX_PLATFORM_HARDWARE == core->GetPlatformType() )
    {
        sts = VideoVPPHW::QueryCaps(core, caps);
        caps.uFrameRateConversion= 1; // "1" means general FRC is supported. "Interpolation" modes descibed by caps.frcCaps
        caps.uDeinterlacing      = 1; // "1" means general deinterlacing is supported
        caps.uVideoSignalInfo    = 1; // "1" means general VSI is supported
        if (sts >= MFX_ERR_NONE)
           return sts;
    }

    caps.iNumBackwardSamples = 1; // fake
    caps.iNumForwardSamples  = 1; // fake

    caps.uAdvancedDI         = 1;
    caps.uDenoiseFilter      = 1;
    caps.uDetailFilter       = 1;
    caps.uFieldWeavingControl= 0;
    caps.uFrameRateConversion= 1; // "1" means general FRC is supported. "Interpolation" modes descibed by caps.frcCaps
    caps.uDeinterlacing      = 1; // "1" means general deinterlacing is supported
    caps.uVideoSignalInfo    = 1; // "1" means general VSI is supported
    caps.uInverseTC          = 1;
    caps.uIStabFilter        = 1;
    caps.uMaxHeight          = 8096;
    caps.uMaxWidth           = 8096;
    caps.uProcampFilter      = 1;
    caps.uSceneChangeDetection = 0;
    caps.uSimpleDI           = 1;
    caps.uVariance           = 0;
    caps.uRotation           = 0;
    caps.uScaling            = 0;

    if (MFX_PLATFORM_HARDWARE == core->GetPlatformType())
        return MFX_WRN_PARTIAL_ACCELERATION;

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPSW::QueryCaps((VideoCORE * core, MfxHwVideoProcessing::mfxVppCaps& caps)


mfxStatus VideoVPPSW::Query(VideoCORE * core, mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    core;

    MFX_CHECK_NULL_PTR1( out );

    if( NULL == in )
    {
        memset(&out->mfx, 0, sizeof(mfxInfoMFX));
        memset(&out->vpp, 0, sizeof(mfxInfoVPP));

        // We have to set FourCC and FrameRate below to
        // pass requirements of CheckPlatformLimitation for frame interpolation

        /* vppIn */
        out->vpp.In.FourCC       = 1;
        out->vpp.In.Height       = 1;
        out->vpp.In.Width        = 1;
        out->vpp.In.PicStruct    = 1;
        out->vpp.In.FrameRateExtN = 1;
        out->vpp.In.FrameRateExtD = 1;

        /* vppOut */
        out->vpp.Out.FourCC       = 1;
        out->vpp.Out.Height       = 1;
        out->vpp.Out.Width        = 1;
        out->vpp.Out.PicStruct    = 1;
        out->vpp.Out.FrameRateExtN = 1;
        out->vpp.Out.FrameRateExtD = 1;

        out->IOPattern           = 1;
        /* protected content is not supported. check it */
        out->Protected           = 1;

        /* AsyncDepth doesn't support for ISV3_2p0 Beta.
        Reason: support by VPP is simple but applications can fails because
        last day test has been added */
        out->AsyncDepth          = 1;

        if (0 == out->NumExtParam)
        {
            out->NumExtParam     = 1;
        }
        else
        {
            // check for IS and AFRC
            out->vpp.In.FourCC         = MFX_FOURCC_NV12;
            out->vpp.Out.FourCC        = MFX_FOURCC_NV12;
            out->vpp.In.FrameRateExtN  = 30;
            out->vpp.Out.FrameRateExtN = 60;
            mfxSts = CheckPlatformLimitations(core, *out, true);
        }
        return mfxSts;
    }
    else
    {
        out->vpp       = in->vpp;

        /* [asyncDepth] section */
        out->AsyncDepth = in->AsyncDepth;

        /* [Protected] section */
        out->Protected = in->Protected;
        if( out->Protected )
        {
            out->Protected = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }

        if(MFX_PLATFORM_HARDWARE == core->GetPlatformType() )
        {
            // hardware protected section
            out->Protected = in->Protected;

            if (IS_PROTECTION_ANY(out->Protected) || 0 == out->Protected)
            {
                mfxSts = MFX_ERR_NONE;
            }
            else
            {
                out->Protected = 0;
                mfxSts = MFX_ERR_UNSUPPORTED;
            }
        }

        /* [IOPattern] section
         * Reuse check function from QueryIOsurf
         * Zero value just skipped.
         */
        mfxU16 inPattern;
        mfxU16 outPattern;
        if (0 == in->IOPattern || MFX_ERR_NONE == CheckIOPattern_AndSetIOMemTypes(in->IOPattern, &inPattern, &outPattern, true))
        {
            out->IOPattern = in->IOPattern;
        }
        else
        {
            mfxSts = MFX_ERR_UNSUPPORTED;
            out->IOPattern = 0;
        }

        /* [ExtParam] section */
        if ((in->ExtParam == 0 && out->ExtParam != 0) ||
            (in->ExtParam != 0 && out->ExtParam == 0) ||
            (in->NumExtParam != out->NumExtParam))
        {
            mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        if (0 != in->ExtParam)
        {
            for (int i = 0; i < in->NumExtParam; i++)
            {
                MFX_CHECK_NULL_PTR1( in->ExtParam[i] );
            }
        }

        if (0 != out->ExtParam)
        {
            for (int i = 0; i < out->NumExtParam; i++)
            {
                MFX_CHECK_NULL_PTR1(out->ExtParam[i]);
            }
        }

        if( out->NumExtParam > MAX_NUM_OF_VPP_EXT_PARAM)
        {
            out->NumExtParam = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }

        if( 0 == out->NumExtParam && in->ExtParam )
        {
            mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        if (in->ExtParam && out->ExtParam && (in->NumExtParam == out->NumExtParam) )
        {
            mfxU16 i;

            // to prevent multiple initialization
            std::vector<mfxU32> filterList(1);
            bool bMultipleInitDNU    = false;
            bool bMultipleInitDOUSE  = false;

            for (i = 0; i < out->NumExtParam; i++)
            {
                if ((in->ExtParam[i] == 0 && out->ExtParam[i] != 0) ||
                    (in->ExtParam[i] != 0 && out->ExtParam[i] == 0))
                {
                    //mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                    mfxSts = MFX_ERR_NULL_PTR;
                    continue; // stop working with ExtParam[i]
                }

                if (in->ExtParam[i] && out->ExtParam[i])
                {
                    if (in->ExtParam[i]->BufferId != out->ExtParam[i]->BufferId)
                    {
                        mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                        continue; // stop working with ExtParam[i]
                    }

                    if (in->ExtParam[i]->BufferSz != out->ExtParam[i]->BufferSz)
                    {
                        mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                        continue; // stop working with ExtParam[i]
                    }

                    // --------------------------------
                    // analysis of configurable filters
                    // --------------------------------
                    if( IsConfigurable( in->ExtParam[i]->BufferId ) )
                    {
                        if( IsFilterFound(&filterList[0], (mfxU32)filterList.size(), in->ExtParam[i]->BufferId) )
                        {
                            mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                        }
                        else
                        {
                            ippsCopy_8u( (Ipp8u*)in->ExtParam[i], (Ipp8u*)out->ExtParam[i], (int)GetConfigSize(in->ExtParam[i]->BufferId) );

                            mfxStatus extSts = ExtendedQuery(core, in->ExtParam[i]->BufferId, out->ExtParam[i]);
                            if( MFX_ERR_NONE != extSts )
                            {
                                mfxSts = extSts;
                            }

                            filterList.push_back(in->ExtParam[i]->BufferId);
                        }

                        continue; // stop working with ExtParam[i]
                    }

                    // --------------------------------
                    // analysis of DONOTUSE structure
                    // --------------------------------
                    else if( MFX_EXTBUFF_VPP_DONOTUSE == in->ExtParam[i]->BufferId )
                    {
                        if( bMultipleInitDNU )
                        {
                            mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                            continue;// stop working with ExtParam[i]
                        }

                        bMultipleInitDNU = true;

                        // deep analysis
                        //--------------------------------------
                        {
                            mfxExtVPPDoNotUse*   extDoNotUseIn  = (mfxExtVPPDoNotUse*)in->ExtParam[i];
                            mfxExtVPPDoNotUse*   extDoNotUseOut = (mfxExtVPPDoNotUse*)out->ExtParam[i];

                            if(extDoNotUseIn->NumAlg != extDoNotUseOut->NumAlg)
                            {
                                mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                                continue; // stop working with ExtParam[i]
                            }

                            if( 0 == extDoNotUseIn->NumAlg )
                            {
                                extDoNotUseIn->NumAlg = 0;

                                mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                                continue; // stop working with ExtParam[i]
                            }
                            if(extDoNotUseIn->NumAlg > 4)
                            {
                                extDoNotUseIn->NumAlg = 0;
                                mfxSts = MFX_ERR_UNSUPPORTED;
                                continue; // stop working with ExtParam[i]
                            }

                            if( NULL == extDoNotUseOut->AlgList || NULL == extDoNotUseIn->AlgList )
                            {
                                mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                                continue; // stop working with ExtParam[i]
                            }

                            for( mfxU32 algIdx = 0; algIdx < extDoNotUseIn->NumAlg; algIdx++ )
                            {
                                // app must turn off filter once only
                                if( IsFilterFound( extDoNotUseIn->AlgList, algIdx, extDoNotUseIn->AlgList[algIdx] ) )
                                {
                                    mfxSts = MFX_ERR_UNSUPPORTED;
                                    continue; // stop working with ExtParam[i]
                                }
                                extDoNotUseOut->AlgList[algIdx] = extDoNotUseIn->AlgList[algIdx];
                            }
                            extDoNotUseOut->NumAlg = extDoNotUseIn->NumAlg;
                        }
                    }

                    // --------------------------------
                    // analysis of DOUSE structure
                    // --------------------------------
                    else if( MFX_EXTBUFF_VPP_DOUSE == in->ExtParam[i]->BufferId )
                    {
                        if( bMultipleInitDOUSE )
                        {
                            mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                            continue;// stop working with ExtParam[i]
                        }

                        bMultipleInitDOUSE = true;

                        // deep analysis
                        //--------------------------------------
                        {
                            mfxExtVPPDoUse*   extDoUseIn  = (mfxExtVPPDoUse*)in->ExtParam[i];
                            mfxExtVPPDoUse*   extDoUseOut = (mfxExtVPPDoUse*)out->ExtParam[i];

                            if(extDoUseIn->NumAlg != extDoUseOut->NumAlg)
                            {
                                mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                                continue; // stop working with ExtParam[i]
                            }

                            if( 0 == extDoUseIn->NumAlg )
                            {
                                extDoUseIn->NumAlg = 0;

                                mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                                continue; // stop working with ExtParam[i]
                            }

                            if( NULL == extDoUseOut->AlgList || NULL == extDoUseIn->AlgList )
                            {
                                mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                                continue; // stop working with ExtParam[i]
                            }

                            for( mfxU32 algIdx = 0; algIdx < extDoUseIn->NumAlg; algIdx++ )
                            {
                                if( !CheckDoUseCompatibility( extDoUseIn->AlgList[algIdx] ) )
                                {
                                    mfxSts = MFX_ERR_UNSUPPORTED;
                                    continue; // stop working with ExtParam[i]
                                }

                                // app must turn off filter once only
                                if( IsFilterFound( extDoUseIn->AlgList, algIdx, extDoUseIn->AlgList[algIdx] ) )
                                {
                                    mfxSts = MFX_ERR_UNSUPPORTED;
                                    continue; // stop working with ExtParam[i]
                                }

                                if(MFX_EXTBUFF_VPP_COMPOSITE == extDoUseIn->AlgList[algIdx])
                                {
                                    mfxSts = MFX_ERR_INVALID_VIDEO_PARAM;
                                    continue; // stop working with ExtParam[i]
                                }

                                if(MFX_EXTBUFF_VPP_FIELD_PROCESSING == extDoUseIn->AlgList[algIdx])
                                {
                                    /* NOTE:
                                     * It's legal to use DOUSE for field processing,
                                     * but application must attach appropriate ext buffer to mfxFrameData for each input surface
                                     */
                                    //mfxSts = MFX_ERR_UNSUPPORTED;
                                    //continue;
                                }
                                extDoUseOut->AlgList[algIdx] = extDoUseIn->AlgList[algIdx];
                            }
                            extDoUseOut->NumAlg = extDoUseIn->NumAlg;
                        }
                        //--------------------------------------
                    }
                    else if( MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION == in->ExtParam[i]->BufferId )
                    {
                        // No specific checks for Opaque ext buffer at the moment.
                    }
                    else
                    {
                        out->ExtParam[i]->BufferId = 0;
                        mfxSts = MFX_ERR_UNSUPPORTED;

                    }// if( MFX_EXTBUFF_VPP_XXX == in->ExtParam[i]->BufferId )

                } // if(in->ExtParam[i] && out->ExtParam[i])

            } //  for (i = 0; i < out->NumExtParam; i++)

        } // if (in->ExtParam && out->ExtParam && (in->NumExtParam == out->NumExtParam) )
        
        if ( out->vpp.In.FourCC  != MFX_FOURCC_P010 && 
             out->vpp.Out.FourCC == MFX_FOURCC_A2RGB10 ){
            if( out->vpp.In.FourCC )
            {
                out->vpp.In.FourCC = 0;
                mfxSts = MFX_ERR_UNSUPPORTED;
            }
        }

        if ( out->vpp.In.FourCC  == MFX_FOURCC_P010 && 
             out->vpp.Out.FourCC != MFX_FOURCC_A2RGB10 &&
             out->vpp.Out.FourCC != MFX_FOURCC_NV12 && 
             out->vpp.Out.FourCC != MFX_FOURCC_P010){
            if( out->vpp.In.FourCC )
            {
                out->vpp.In.FourCC = 0;
                mfxSts = MFX_ERR_UNSUPPORTED;
            }
        }

        /* [IN VPP] data */
        if( out->vpp.In.FourCC != MFX_FOURCC_YV12 &&
            out->vpp.In.FourCC != MFX_FOURCC_NV12 &&
            out->vpp.In.FourCC != MFX_FOURCC_YUY2 &&
            out->vpp.In.FourCC != MFX_FOURCC_RGB4 && 
            out->vpp.In.FourCC != MFX_FOURCC_P010 )
        {
            if( out->vpp.In.FourCC )
            {
                out->vpp.In.FourCC = 0;
                mfxSts = MFX_ERR_UNSUPPORTED;
            }
        }

        if (MFX_FOURCC_NV12    != out->vpp.Out.FourCC && 
            MFX_FOURCC_RGB4    != out->vpp.Out.FourCC &&
            MFX_FOURCC_P010    != out->vpp.Out.FourCC &&
            MFX_FOURCC_A2RGB10 != out->vpp.Out.FourCC)
        {
            out->vpp.Out.FourCC = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }

        if (out->vpp.In.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
            out->vpp.In.PicStruct != MFX_PICSTRUCT_FIELD_TFF   &&
            out->vpp.In.PicStruct != MFX_PICSTRUCT_FIELD_BFF   &&
            out->vpp.In.PicStruct != MFX_PICSTRUCT_UNKNOWN)
        {

            if( out->vpp.In.PicStruct )
            {
                out->vpp.In.PicStruct = 0;
                mfxSts = MFX_ERR_UNSUPPORTED;
            }
        }

        if ( !(out->vpp.In.FrameRateExtN * out->vpp.In.FrameRateExtD) &&
            (out->vpp.In.FrameRateExtN + out->vpp.In.FrameRateExtD) )
        {
            out->vpp.In.FrameRateExtN = 0;
            out->vpp.In.FrameRateExtD = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }

        if( out->vpp.In.Width )
        {
            if ( (out->vpp.In.Width & 15 ) != 0 )
            {
                out->vpp.In.Width = 0;
                mfxSts = MFX_ERR_UNSUPPORTED;
            }
        }

        if (out->vpp.In.Height)
        {
            if (MFX_PICSTRUCT_PROGRESSIVE == out->vpp.In.PicStruct)
            {
                if ((out->vpp.In.Height  & 15) !=0 )
                {
                    out->vpp.In.Height = 0;
                    mfxSts = MFX_ERR_UNSUPPORTED;
                }
            }
            else
            { // TFF / BFF/ UNKNOWN
                if ((out->vpp.In.Height  & 15) !=0 )// in according with internal spec rev 22583
                {
                    out->vpp.In.Height = 0;
                    mfxSts = MFX_ERR_UNSUPPORTED;
                }
            }
        }

        /* [OUT VPP] data */
        if( out->vpp.Out.FourCC != MFX_FOURCC_YV12 &&
            out->vpp.Out.FourCC != MFX_FOURCC_NV12 &&
            out->vpp.Out.FourCC != MFX_FOURCC_RGB4 && 
            out->vpp.Out.FourCC != MFX_FOURCC_P010 &&
            out->vpp.Out.FourCC != MFX_FOURCC_A2RGB10 )
        {
            out->vpp.Out.FourCC = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }

        if (out->vpp.Out.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
            out->vpp.Out.PicStruct != MFX_PICSTRUCT_FIELD_TFF   &&
            out->vpp.Out.PicStruct != MFX_PICSTRUCT_FIELD_BFF   &&
            out->vpp.Out.PicStruct != MFX_PICSTRUCT_UNKNOWN)
        {
            if(out->vpp.Out.PicStruct)
            {
                out->vpp.Out.PicStruct = 0;
                mfxSts = MFX_ERR_UNSUPPORTED;
            }
        }

        if ( !(out->vpp.Out.FrameRateExtN * out->vpp.Out.FrameRateExtD) &&
            (out->vpp.Out.FrameRateExtN + out->vpp.Out.FrameRateExtD))
        {
            out->vpp.Out.FrameRateExtN = 0;
            out->vpp.Out.FrameRateExtD = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }

        if ( out->vpp.Out.Width )
        {
            if ( (out->vpp.Out.Width & 15 ) != 0 )
            {
                mfxSts = MFX_ERR_UNSUPPORTED;
            }
        }

        if( out->vpp.Out.Height )
        {
            if (MFX_PICSTRUCT_PROGRESSIVE == out->vpp.Out.PicStruct)
            {
                if ((out->vpp.Out.Height  & 15) !=0)
                {
                    out->vpp.Out.Height = 0;
                    mfxSts = MFX_ERR_UNSUPPORTED;
                }
            }
            else
            { // TFF or BFF
                if ((out->vpp.Out.Height  & 15) !=0) // in according with internal spec (ISV3b-SNBa rev. 22583)
                {
                    out->vpp.Out.Height = 0;
                    mfxSts = MFX_ERR_UNSUPPORTED;
                }
            }
        }

        //-------------------------------------------------
        // FRC, IS and similar enhancement algorithms
        // special "interface" to signal on application level about support/unsupport ones
        bool bCorrectionEnable = true;
        mfxStatus localSts = CheckPlatformLimitations(core, *out, bCorrectionEnable);
        //-------------------------------------------------

        if (mfxSts != MFX_ERR_NONE)
        {
            return mfxSts;
        }

        if(MFX_PLATFORM_HARDWARE == core->GetPlatformType() )
        {
            // HW VPP checking
            VideoVPPHW hwVPP(VideoVPPHW::ALL, core);
            mfxStatus sts = hwVPP.Init(out, true);
            hwVPP.Close();

            // Statuses returned by Init differ in several cases from Query
            if (MFX_ERR_INVALID_VIDEO_PARAM == sts ||
                MFX_ERR_UNSUPPORTED == sts)
            {
                sts = MFX_ERR_UNSUPPORTED;
                return sts;
            }
            if (MFX_ERR_NONE != sts && IS_PROTECTION_ANY(out->Protected))
            {
                out->Protected = 0;
                return MFX_ERR_UNSUPPORTED;
            }

            if(MFX_ERR_NONE == sts)
            {
                return localSts;
            }
            else
            {
                return MFX_WRN_PARTIAL_ACCELERATION;
            }
        }
        else
        {
            return  localSts;
        }
    }//else
} // mfxStatus VideoVPPSW::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)


mfxStatus VideoVPPSW::Reset(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1( par );

    VPP_CHECK_NOT_INITIALIZED;

    sts = CheckFrameInfo( &(par->vpp.In),  VPP_IN );
    MFX_CHECK_STS( sts );

    sts = CheckFrameInfo( &(par->vpp.Out), VPP_OUT );
    MFX_CHECK_STS(sts);

    //-----------------------------------------------------
    // specific check for Reset()
    if( m_InitState.In.PicStruct != par->vpp.In.PicStruct || m_InitState.Out.PicStruct != par->vpp.Out.PicStruct)
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    /* IOPattern check */
    if( m_InitState.IOPattern != par->IOPattern )
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    /* Protected Check */
    sts = CheckProtectedMode( par->Protected );
    MFX_CHECK_STS(sts);

    /* AsyncDepth */
    if( m_InitState.AsyncDepth < par->AsyncDepth )
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    /* in general, in/out resolution should be <= m_initParam.resolution */
    if( (par->vpp.In.Width  > m_InitState.In.Width)  || (par->vpp.In.Height  > m_InitState.In.Height) ||
        (par->vpp.Out.Width > m_InitState.Out.Width) || (par->vpp.Out.Height > m_InitState.Out.Height) )
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }
    //-----------------------------------------------------

    /* Opaque */
    if( m_bOpaqMode[VPP_IN] || m_bOpaqMode[VPP_OUT] )
    {
        bool bLocalOpaqMode[2] = {false, false};

        sts = CheckOpaqMode( par, bLocalOpaqMode );
        MFX_CHECK_STS( sts );

        if( bLocalOpaqMode[VPP_IN] && !m_bOpaqMode[VPP_IN] )
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }

        if( bLocalOpaqMode[VPP_OUT] && !m_bOpaqMode[VPP_OUT] )
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }

        if( bLocalOpaqMode[VPP_IN] || bLocalOpaqMode[VPP_OUT] )
        {
            mfxFrameAllocRequest localOpaqRequest[2];
            sts = GetOpaqRequest( par, bLocalOpaqMode, localOpaqRequest);
            MFX_CHECK_STS( sts );

            if( bLocalOpaqMode[VPP_IN] )
            {
                if ( m_requestOpaq[VPP_IN].NumFrameMin != localOpaqRequest[VPP_IN].NumFrameMin ||
                    m_requestOpaq[VPP_IN].NumFrameSuggested != localOpaqRequest[VPP_IN].NumFrameSuggested )
                {
                    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
                }
            }
            if( bLocalOpaqMode[VPP_OUT] )
            {
                if ( m_requestOpaq[VPP_OUT].NumFrameMin != localOpaqRequest[VPP_OUT].NumFrameMin ||
                    m_requestOpaq[VPP_OUT].NumFrameSuggested != localOpaqRequest[VPP_OUT].NumFrameSuggested )
                {
                    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
                }
            }
        }
    }// Opaque


    if (m_pHWVPP.get())
    {
        sts = m_pHWVPP.get()->Reset(par);

        if (MFX_ERR_NONE != sts && IS_PROTECTION_ANY(par->Protected))
        {
            return MFX_ERR_UNSUPPORTED;
        }

        if ((MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY) != par->IOPattern &&
            IS_PROTECTION_ANY(par->Protected))
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        MFX_CHECK_STS(sts);

        bool bCorrectionEnable = false;
        sts = CheckPlatformLimitations(m_core, *par, bCorrectionEnable);

    }
    else
    {
        /* ExtParam */
        mfxVideoParam filtParam;
        filtParam.ExtParam    = par->ExtParam;
        filtParam.NumExtParam = par->NumExtParam;

        /* reset every filter */
        filtParam.vpp.Out = par->vpp.In;

        for(mfxU32 filterIndex = 0; filterIndex < GetNumUsedFilters(); filterIndex++ )
        {
            filtParam.vpp.In = filtParam.vpp.Out;

            switch ( m_pipelineList[filterIndex] )
            {
            case MFX_EXTBUFF_VPP_CSC:
                {
                    filtParam.vpp.Out.FourCC = MFX_FOURCC_NV12;
                    if ( MFX_FOURCC_P010 == par->vpp.Out.FourCC  ||
                         MFX_FOURCC_P210 == par->vpp.Out.FourCC  ||
                         MFX_FOURCC_NV16 == par->vpp.Out.FourCC  ||
                         MFX_FOURCC_A2RGB10 == par->vpp.Out.FourCC  )
                    {
                        filtParam.vpp.Out.FourCC = par->vpp.Out.FourCC;
                        filtParam.vpp.Out.Shift = par->vpp.Out.Shift;
                    }
                    break;
                }

            case MFX_EXTBUFF_VPP_CSC_OUT_RGB4:
                {
                    filtParam.vpp.Out.FourCC = MFX_FOURCC_RGB4;
                    break;
                }

            case MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO:
                {
                    filtParam.vpp.Out.FourCC = par->vpp.Out.FourCC;
                    break;
                }

            case MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10:
                {
                    filtParam.vpp.Out.FourCC = MFX_FOURCC_A2RGB10;
                    break;
                }

            case MFX_EXTBUFF_VPP_DI_30i60p:
            case MFX_EXTBUFF_VPP_ITC:
                filtParam.vpp.Out.FrameRateExtD = par->vpp.Out.FrameRateExtD;
                filtParam.vpp.Out.FrameRateExtN = par->vpp.Out.FrameRateExtN;

            case MFX_EXTBUFF_VPP_DI:
            case MFX_EXTBUFF_VPP_DEINTERLACING:
                {
                    filtParam.vpp.Out.PicStruct = par->vpp.Out.PicStruct;
                    break;
                }

            case MFX_EXTBUFF_VPP_RESIZE:
                {
                    filtParam.vpp.Out.Width  = par->vpp.Out.Width;
                    filtParam.vpp.Out.Height = par->vpp.Out.Height;
                    break;
                }

            case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
                {
                    filtParam.vpp.Out.FrameRateExtD = par->vpp.Out.FrameRateExtD;
                    filtParam.vpp.Out.FrameRateExtN = par->vpp.Out.FrameRateExtN;
                    break;
                }

            default: // DN, SA, PA, DTL, GAMUT
                {
                    // nothing to do
                    break;
                }
            }

            sts = m_ppFilters[filterIndex]->Reset( &filtParam );

            MFX_CHECK_STS(sts);
        }

        bool bCorrectionEnable = false;
        sts = CheckPlatformLimitations(m_core, *par, bCorrectionEnable);

        if (MFX_ERR_UNSUPPORTED == sts)
        {
            sts = MFX_ERR_INVALID_VIDEO_PARAM;
        }
        VPP_RESET;
    }

    /* save init params to prevent core crash */
    m_errPrtctState.In  = par->vpp.In;
    m_errPrtctState.Out = par->vpp.Out;
    m_errPrtctState.IOPattern  = par->IOPattern;
    m_errPrtctState.AsyncDepth = par->AsyncDepth;

    mfxStatus compSts = GetCompositionEnabledStatus(par);
    if (compSts == MFX_ERR_NONE)
        m_errPrtctState.isCompositionModeEnabled = true;
    else
        m_errPrtctState.isCompositionModeEnabled = false;

    return sts;

} // mfxStatus VideoVPPSW::Reset(mfxVideoParam *par)


mfxTaskThreadingPolicy VideoVPPSW::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_INTRA;

} // mfxTaskThreadingPolicy VideoVPPSW::GetThreadingPolicy(void)

//---------------------------------------------------------
//                            UTILS
//---------------------------------------------------------
void ConvertCaps2ListDoUse(MfxHwVideoProcessing::mfxVppCaps& caps, std::vector<mfxU32>& list)
{
    if(caps.uProcampFilter)
    {
        list.push_back(MFX_EXTBUFF_VPP_PROCAMP);
    }

    if(caps.uDenoiseFilter)
    {
        list.push_back(MFX_EXTBUFF_VPP_DENOISE);
    }

    if(caps.uDetailFilter)
    {
        list.push_back(MFX_EXTBUFF_VPP_DETAIL);
    }

    if(caps.uFrameRateConversion)
    {
        list.push_back(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION);
    }

    if(caps.uDeinterlacing)
    {
        list.push_back(MFX_EXTBUFF_VPP_DEINTERLACING);
    }

    if(caps.uVideoSignalInfo)
    {
        list.push_back(MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO);
    }

    if(caps.uIStabFilter)
    {
        list.push_back(MFX_EXTBUFF_VPP_IMAGE_STABILIZATION);
    }

    if(caps.uVariance)
    {
        list.push_back(MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION);
    }

    if(caps.uRotation)
    {
        list.push_back(MFX_EXTBUFF_VPP_ROTATION);
    }

    if(caps.uScaling)
    {
        list.push_back(MFX_EXTBUFF_VPP_SCALING);
    }

    /* FIELD Copy is always present*/
    list.push_back(MFX_EXTBUFF_VPP_FIELD_PROCESSING);
    /* Composition is always present*/
    list.push_back(MFX_EXTBUFF_VPP_COMPOSITE);

} // void ConvertCaps2ListDoUse(MfxHwVideoProcessing::mfxVppCaps& caps, std::vector<mfxU32> list)


mfxStatus VideoVPPSW::CheckPlatformLimitations(
    VideoCORE* core,
    mfxVideoParam & param,
    bool bCorrectionEnable)
{
    std::vector<mfxU32> capsList;
    //{
        MfxHwVideoProcessing::mfxVppCaps caps;
        QueryCaps(core, caps);
        ConvertCaps2ListDoUse(caps, capsList);
    //}

    std::vector<mfxU32> pipelineList;
    bool bExtendedSupport = true;
    mfxStatus sts = GetPipelineList( &param, pipelineList, bExtendedSupport );
    MFX_CHECK_STS(sts);

    std::vector<mfxU32> supportedList;
    std::vector<mfxU32> unsupportedList;

    // compare pipelineList and capsList
    mfxStatus capsSts = GetCrossList(pipelineList, capsList, supportedList, unsupportedList);// this function could return WRN_FILTER_SKIPPED

    if (MFX_PLATFORM_SOFTWARE == core->GetPlatformType() )
    {
        sts = CheckLimitationsSW(param, supportedList, bCorrectionEnable);// this function could return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM and WRN_FILTER_SKIPPED
    }
    else
    {
        sts = CheckLimitationsHW(param, supportedList, bCorrectionEnable);// this function could return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM and WRN_FILTER_SKIPPED
    }

    // check unsupported list if we need to reset ext buffer fields
    if(!unsupportedList.empty())
    {
        if (IsFilterFound(&unsupportedList[0], (mfxU32)unsupportedList.size(), MFX_EXTBUFF_VPP_IMAGE_STABILIZATION))
        {
            SetMFXISMode(param, 0);
        }
    }

    return (MFX_ERR_NONE != capsSts) ? capsSts : sts;

} // mfxStatus CheckPlatformLimitations(...)

#endif // MFX_ENABLE_VPP
/* EOF */

