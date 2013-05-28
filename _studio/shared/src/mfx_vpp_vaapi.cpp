/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined (MFX_VA_LINUX)

#include "mfx_vpp_defs.h"
#include "mfx_vpp_vaapi.h"
#include "mfx_utils.h"
#include "libmfx_core_vaapi.h"

enum QueryStatus
{
    VPREP_GPU_READY         =   0,
    VPREP_GPU_BUSY          =   1,
    VPREP_GPU_NOT_REACHED   =   2,
    VPREP_GPU_FAILED        =   3
};

//////////////////////////////////////////////////////////////////////////
using namespace MfxHwVideoProcessing;

static float convertValue(const float OldMin,const float OldMax,const float NewMin,const float NewMax,const float OldValue)
{
    if((0 == NewMin) && (0 == NewMax)) return OldValue; //workaround
    float oldRange = OldMax - OldMin;
    float newRange = NewMax - NewMin;
    return (((OldValue - OldMin) * newRange) / oldRange) + NewMin;
}

#define DEFAULT_HUE 0
#define DEFAULT_SATURATION 1
#define DEFAULT_CONTRAST 1
#define DEFAULT_BRIGHTNESS 0

VAAPIVideoProcessing::VAAPIVideoProcessing():
  m_bRunning(false)
, m_core(NULL)
, m_vaDisplay(0)
, m_vaConfig(0)
, m_vaContextVPP(0)
, m_denoiseFilterID(VA_INVALID_ID)
, m_deintFilterID(VA_INVALID_ID)
, m_numFilterBufs(0)
{

    for(int i = 0; i < VAProcFilterCount; i++)
        m_filterBufs[i] = VA_INVALID_ID;

    memset( (void*)&m_denoiseCaps, 0, sizeof(m_denoiseCaps));
    memset( (void*)&m_deinterlacingCaps, 0, sizeof(m_deinterlacingCaps));

    m_cachedReadyTaskIndex.clear();
    m_feedbackCache.clear();

} // VAAPIVideoProcessing::VAAPIVideoProcessing()


VAAPIVideoProcessing::~VAAPIVideoProcessing()
{
    Close();

} // VAAPIVideoProcessing::~VAAPIVideoProcessing()

mfxStatus VAAPIVideoProcessing::CreateDevice(VideoCORE * core, mfxVideoParam* pParams, bool /*isTemporal*/)
{
    MFX_CHECK_NULL_PTR1( core );

    VAAPIVideoCORE* hwCore = dynamic_cast<VAAPIVideoCORE*>(core);

    MFX_CHECK_NULL_PTR1( hwCore );

    mfxStatus sts = hwCore->GetVAService( &m_vaDisplay);
    MFX_CHECK_STS( sts );

    sts = Init( &m_vaDisplay, pParams);

    MFX_CHECK_STS(sts);

    m_cachedReadyTaskIndex.clear();

    m_core = core;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::CreateDevice(VideoCORE * core, mfxInitParams* pParams)


mfxStatus VAAPIVideoProcessing::DestroyDevice(void)
{
    mfxStatus sts = Close();

    return sts;

} // mfxStatus VAAPIVideoProcessing::DestroyDevice(void)


mfxStatus VAAPIVideoProcessing::Close(void)
{
    if( m_vaContextVPP )
    {
        vaDestroyContext( m_vaDisplay, m_vaContextVPP );
        m_vaContextVPP = 0;
    }

    if( m_vaConfig )
    {
        vaDestroyConfig( m_vaDisplay, m_vaConfig );
        m_vaConfig = 0;
    }

    if( VA_INVALID_ID != m_denoiseFilterID )
    {
        vaDestroyBuffer(m_vaDisplay, m_denoiseFilterID);
    }

    if( VA_INVALID_ID != m_deintFilterID )
    {
        vaDestroyBuffer(m_vaDisplay, m_deintFilterID);
    }

    for(int i = 0; i < VAProcFilterCount; i++)
        m_filterBufs[i] = VA_INVALID_ID;

    m_denoiseFilterID   = VA_INVALID_ID;
    m_deintFilterID     = VA_INVALID_ID;

    memset( (void*)&m_denoiseCaps, 0, sizeof(m_denoiseCaps));
    memset( (void*)&m_deinterlacingCaps, 0, sizeof(m_deinterlacingCaps));

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::Close(void)

mfxStatus VAAPIVideoProcessing::Init(_mfxPlatformAccelerationService* pVADisplay, mfxVideoParam* pParams)
{
    if(false == m_bRunning)
    {
        MFX_CHECK_NULL_PTR1( pVADisplay );
        MFX_CHECK_NULL_PTR1( pParams );

        m_cachedReadyTaskIndex.clear();

        VAEntrypoint* va_entrypoints = NULL;
        VAStatus vaSts;
        int va_max_num_entrypoints   = vaMaxNumEntrypoints(m_vaDisplay);
        if(va_max_num_entrypoints)
            va_entrypoints = (VAEntrypoint*)ippsMalloc_8u(va_max_num_entrypoints*sizeof(VAEntrypoint));
        else
            return MFX_ERR_DEVICE_FAILED;

        mfxI32 entrypointsCount = 0, entrypointsIndx = 0;

        vaSts = vaQueryConfigEntrypoints(m_vaDisplay,
                                            VAProfileNone,
                                            va_entrypoints,
                                            &entrypointsCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        for( entrypointsIndx = 0; entrypointsIndx < entrypointsCount; entrypointsIndx++ )
        {
            if( VAEntrypointVideoProc == va_entrypoints[entrypointsIndx] )
            {
                m_bRunning = true;
                break;
            }
        }
        ippsFree(va_entrypoints);

        if( !m_bRunning )
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Configuration
        VAConfigAttrib va_attributes;
        vaSts = vaGetConfigAttributes(m_vaDisplay, VAProfileNone, VAEntrypointVideoProc, &va_attributes, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaCreateConfig( m_vaDisplay,
                                VAProfileNone,
                                VAEntrypointVideoProc,
                                &va_attributes,
                                1,
                                &m_vaConfig);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        // Context
        int width = pParams->vpp.Out.Width;
        int height = pParams->vpp.Out.Height;

        vaSts = vaCreateContext(m_vaDisplay,
                                m_vaConfig,
                                width,
                                height,
                                VA_PROGRESSIVE,
                                0, 0,
                                &m_vaContextVPP);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::Init(_mfxPlatformAccelerationService* pVADisplay, mfxVideoParam* pParams)


mfxStatus VAAPIVideoProcessing::Register(
    mfxHDLPair* /*pSurfaces*/,
    mfxU32 /*num*/,
    BOOL /*bRegister*/)
{
    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Register(_mfxPlatformVideoSurface *pSurfaces, ...)


mfxStatus VAAPIVideoProcessing::QueryCapabilities(mfxVppCaps& caps)
{
    VAStatus vaSts;
    memset(&caps,  0, sizeof(mfxVppCaps));

    VAProcFilterType filters[VAProcFilterCount];
    mfxU32 num_filters = VAProcFilterCount;

    vaSts = vaQueryVideoProcFilters(m_vaDisplay, m_vaContextVPP, filters, &num_filters);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_denoise_caps = 1;
    vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterNoiseReduction,
                               &m_denoiseCaps, &num_denoise_caps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_deinterlacing_caps = VAProcDeinterlacingCount;
    vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterDeinterlacing,
                               &m_deinterlacingCaps, &num_deinterlacing_caps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for( mfxU32 filtersIndx = 0; filtersIndx < num_filters; filtersIndx++ )
    {
        if (filters[filtersIndx])
        {
            switch (filters[filtersIndx])
            {
                case VAProcFilterNoiseReduction:
                    caps.uDenoiseFilter = 1;
                    break;
                case VAProcFilterDeinterlacing:
                    for (mfxU32 i = 0; i < num_deinterlacing_caps; i++)
                    {
                        if (VAProcDeinterlacingBob == m_deinterlacingCaps[i].type)
                            caps.uSimpleDI = 1;
                        if (VAProcDeinterlacingWeave == m_deinterlacingCaps[i].type           ||
                            VAProcDeinterlacingMotionAdaptive == m_deinterlacingCaps[i].type  ||
                            VAProcDeinterlacingMotionCompensated == m_deinterlacingCaps[i].type)
                            caps.uAdvancedDI = 1;
                    }
                    break;
                case VAProcFilterColorBalance:
                    caps.uProcampFilter = 1;
                    break;
               default:
                    break;
            }
        }
    }

    caps.uMaxWidth  = 4096;
    caps.uMaxHeight = 4096;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::QueryCapabilities(mfxVppCaps& caps)


mfxStatus VAAPIVideoProcessing::Execute(mfxExecuteParams *pParams)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "VPP Execute");

    VAStatus vaSts = VA_STATUS_SUCCESS;

    MFX_CHECK_NULL_PTR1( pParams );

    MFX_CHECK_NULL_PTR1( pParams->targetSurface.hdl.first );

    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces );

    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces[0].hdl.first );

    if (VA_INVALID_ID == m_deintFilterID)
    {
        if (pParams->iDeinterlacingAlgorithm)
        {
            VAProcFilterParameterBufferDeinterlacing deint;
            deint.type                   = VAProcFilterDeinterlacing;
            //WA for VPG driver. Need to rewrite it with caps usage when driver begins to return a correct list of supported DI algorithms
#ifndef MFX_VA_ANDROID
            //deint.algorithm              = pParams->iDeinterlacingAlgorithm == 1 ? VAProcDeinterlacingBob : VAProcDeinterlacingMotionAdaptive;
            /*
             * Per agreement used "VAProcDeinterlacingBob" DI type as default value
             * */
            deint.algorithm              = VAProcDeinterlacingBob;
            mfxDrvSurface* pRefSurf_frameInfo = &(pParams->pRefSurfaces[0]);
            switch (pRefSurf_frameInfo->frameInfo.PicStruct)
            {
                case MFX_PICSTRUCT_PROGRESSIVE:
                    deint.flags = VA_DEINTERLACING_ONE_FIELD;
                    break;
                    /* See comment form va_vpp.h
                     * */
                    /**
                     * \brief Bottom field used in deinterlacing.
                     * if this is not set then assumes top field is used.
                     */

                //case MFX_PICSTRUCT_FIELD_TFF:
                //    deint.flags = VA_DEINTERLACING_ONE_FIELD;
                //    break;
                case MFX_PICSTRUCT_FIELD_BFF:
                    deint.flags = VA_DEINTERLACING_BOTTOM_FIELD;
                    break;
            }
#endif
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextVPP,
                                   VAProcFilterParameterBufferType,
                                   sizeof(deint), 1,
                                   &deint, &m_deintFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_deintFilterID;
        }
    }

    if (VA_INVALID_ID == m_denoiseFilterID)
    {
        if (pParams->denoiseFactor || true == pParams->bDenoiseAutoAdjust)
        {
            VAProcFilterParameterBuffer denoise;
            denoise.type  = VAProcFilterNoiseReduction;
            if(pParams->bDenoiseAutoAdjust)
                denoise.value = m_denoiseCaps.range.default_value;
            else
                denoise.value = convertValue(0,
                                            100,
                                              m_denoiseCaps.range.min_value,
                                              m_denoiseCaps.range.max_value,
                                              pParams->denoiseFactor);
            vaSts = vaCreateBuffer(m_vaDisplay,
                          m_vaContextVPP,
                          VAProcFilterParameterBufferType,
                          sizeof(denoise), 1,
                          &denoise, &m_denoiseFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_denoiseFilterID;
        }
    }

    // mfxU32 SampleCount = pParams->refCount;
    // WA:
    mfxU32 SampleCount = 1;

    m_pipelineParam.resize(SampleCount);
    m_pipelineParamID.resize(SampleCount);

    std::vector<VARectangle> input_region;
    input_region.resize(SampleCount);
    std::vector<VARectangle> output_region;
    output_region.resize(SampleCount);

    for( mfxU32 refIdx = 0; refIdx < SampleCount; refIdx++ )
    {
        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[refIdx]);

        VASurfaceID* srf = (VASurfaceID*)(pRefSurf->hdl.first);
        m_pipelineParam[refIdx].surface = *srf;

        // source cropping
        mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
        input_region[refIdx].y   = inInfo->CropY;
        input_region[refIdx].x   = inInfo->CropX;
        input_region[refIdx].height = inInfo->CropH;
        input_region[refIdx].width  = inInfo->CropW;
        m_pipelineParam[refIdx].surface_region = &input_region[refIdx];

        // destination cropping
        mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);
        output_region[refIdx].y  = outInfo->CropY;
        output_region[refIdx].x   = outInfo->CropX;
        output_region[refIdx].height= outInfo->CropH;
        output_region[refIdx].width  = outInfo->CropW;
        m_pipelineParam[refIdx].output_region = &output_region[refIdx];

        m_pipelineParam[refIdx].output_background_color = 0xff108080; // black for yuv

        mfxU32  refFourcc = pRefSurf->frameInfo.FourCC;
        switch (refFourcc)
        {
        case MFX_FOURCC_RGB4:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardNone;
            break;
        case MFX_FOURCC_NV12:  //VA_FOURCC_NV12:
        default:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardBT601;
            break;
        }

        mfxU32  targetFourcc = pParams->targetSurface.frameInfo.FourCC;
        switch (targetFourcc)
        {
        case MFX_FOURCC_RGB4:
            m_pipelineParam[refIdx].output_color_standard = VAProcColorStandardNone;
            break;
        case MFX_FOURCC_NV12:
        default:
            m_pipelineParam[refIdx].output_color_standard = VAProcColorStandardBT601;
            break;
        }

        //m_pipelineParam[refIdx].pipeline_flags = ?? //VA_PROC_PIPELINE_FAST or VA_PROC_PIPELINE_SUBPICTURES

        switch (pRefSurf->frameInfo.PicStruct)
        {
            case MFX_PICSTRUCT_PROGRESSIVE:
                m_pipelineParam[refIdx].filter_flags = VA_FRAME_PICTURE;
                break;
            case MFX_PICSTRUCT_FIELD_TFF:
                m_pipelineParam[refIdx].filter_flags = VA_TOP_FIELD;
                break;
            case MFX_PICSTRUCT_FIELD_BFF:
                m_pipelineParam[refIdx].filter_flags = VA_BOTTOM_FIELD;
                break;
        }

        m_pipelineParam[refIdx].filters  = m_filterBufs;
        m_pipelineParam[refIdx].num_filters  = m_numFilterBufs;

        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextVPP,
                            VAProcPipelineParameterBufferType,
                            sizeof(VAProcPipelineParameterBuffer),
                            1,
                            &m_pipelineParam[refIdx],
                            &m_pipelineParamID[refIdx]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
        vaSts = vaBeginPicture(m_vaDisplay,
                            m_vaContextVPP,
                            *outputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
        for( mfxU32 refIdx = 0; refIdx < SampleCount; refIdx++ )
        {
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[refIdx], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaEndPicture");
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextVPP);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    for( mfxU32 refIdx = 0; refIdx < SampleCount; refIdx++ )
    {
        if ( m_pipelineParamID[refIdx] != VA_INVALID_ID)
        {
            vaDestroyBuffer(m_vaDisplay, m_pipelineParamID[refIdx]);
            m_pipelineParamID[refIdx] = VA_INVALID_ID;
        }
    }

    // (3) info needed for sync operation
    //-------------------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback; // {surface & number_of_task}
        currentFeedback.surface = *outputSurface;
        currentFeedback.number = pParams->statusReportID;
        m_feedbackCache.push_back(currentFeedback);
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Execute(FASTCOMP_BLT_PARAMS *pVideoCompositingBlt)

mfxStatus VAAPIVideoProcessing::QueryTaskStatus(mfxU32 taskIndex)
{
    VAStatus vaSts;
    FASTCOMP_QUERY_STATUS queryStatus;

    // (1) find params (sutface & number) are required by feedbackNumber
    //-----------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        std::vector<ExtVASurface>::iterator iter = m_feedbackCache.begin();
        while(iter != m_feedbackCache.end())
        {
            ExtVASurface currentFeedback = *iter;

            VASurfaceStatus surfSts = VASurfaceSkipped;

            vaSts = vaQuerySurfaceStatus(m_vaDisplay,  currentFeedback.surface, &surfSts);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            switch (surfSts)
            {
                case VASurfaceReady:
                    queryStatus.QueryStatusID = currentFeedback.number;
                    queryStatus.Status = VPREP_GPU_READY;
                    m_cachedReadyTaskIndex.insert(queryStatus.QueryStatusID);
                    m_feedbackCache.erase(iter);
                    break;
                case VASurfaceRendering:
                case VASurfaceDisplaying:
                    ++iter;
                    break;
                case VASurfaceSkipped:
                default:
                    assert(!"bad feedback status");
                    return MFX_ERR_DEVICE_FAILED;
            }
        }

        std::set<mfxU32>::iterator iterator = m_cachedReadyTaskIndex.find(taskIndex);

        if (m_cachedReadyTaskIndex.end() == iterator)
        {
            return MFX_TASK_BUSY;
        }

        m_cachedReadyTaskIndex.erase(iterator);
    }

    return MFX_TASK_DONE;

} // mfxStatus FastCompositingDDI::QueryTaskStatus(mfxU32 taskIndex)

mfxStatus VAAPIVideoProcessing::QueryTaskStatus(FASTCOMP_QUERY_STATUS *pQueryStatus, mfxU32 numStructures)
{
//     VASurfaceID waitSurface;
//
//     // (1) find params (sutface & number) are required by feedbackNumber
//     //-----------------------------------------------
//     {
//         UMC::AutomaticUMCMutex guard(m_guard);
//
//         bool isFound  = false;
//         int num_element = m_feedbackCache.size();
//         for( mfxU32 indxSurf = 0; indxSurf < num_element; indxSurf++ )
//         {
//             //ExtVASurface currentFeedback = m_feedbackCache.pop();
//             ExtVASurface currentFeedback = m_feedbackCache[indxSurf];
//
//             // (2) Syncronization by output (target surface)
//             //-----------------------------------------------
//             VAStatus vaSts = vaSyncSurface(m_vaDisplay, currentFeedback.surface);
//             MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//
//             VASurfaceStatus surfSts = VASurfaceReady;
//
//             vaSts = vaQuerySurfaceStatus(m_vaDisplay,  currentFeedback.surface, &surfSts);
//             MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//
//             // update for comp vpp_hw
//
//             pQueryStatus[indxSurf].StatusReportID = currentFeedback.number;
//             pQueryStatus[indxSurf].Status = VPREP_GPU_READY;
//
//             m_feedbackCache.erase( m_feedbackCache.begin() );
//         }
//
//     }
    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::QueryTaskStatus(PREPROC_QUERY_STATUS *pQueryStatus, mfxU32 numStructures)

#endif // #if defined (MFX_VA_LINUX)
#endif // #if defined (MFX_VPP_ENABLE)
/* EOF */
