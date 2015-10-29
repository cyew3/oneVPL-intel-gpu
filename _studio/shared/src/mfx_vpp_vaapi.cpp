/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined (MFX_VA_LINUX)

#include <math.h>
#include "mfx_vpp_defs.h"
#include "mfx_vpp_vaapi.h"
#include "mfx_utils.h"
#include "libmfx_core_vaapi.h"
#include "ippcore.h"
#include "ippi.h"
#include <algorithm>

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
, m_detailFilterID(VA_INVALID_ID)
, m_deintFilterID(VA_INVALID_ID)
, m_procampFilterID(VA_INVALID_ID)
, m_frcFilterID(VA_INVALID_ID)
, m_refCountForADI(0)
, m_bFakeOutputEnabled(false)
, m_numFilterBufs(0)
, m_primarySurface4Composition(NULL)
{

    for(int i = 0; i < VAProcFilterCount; i++)
        m_filterBufs[i] = VA_INVALID_ID;

    memset( (void*)&m_pipelineCaps, 0, sizeof(m_pipelineCaps));
    memset( (void*)&m_denoiseCaps, 0, sizeof(m_denoiseCaps));
    memset( (void*)&m_detailCaps, 0, sizeof(m_detailCaps));
    memset( (void*)&m_deinterlacingCaps, 0, sizeof(m_deinterlacingCaps));
    memset( (void*)&m_frcCaps, 0, sizeof(m_frcCaps));
    memset( (void*)&m_procampCaps, 0, sizeof(m_procampCaps));

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
    if (m_primarySurface4Composition != NULL)
    {
        vaDestroySurfaces(m_vaDisplay,m_primarySurface4Composition,1);
        free(m_primarySurface4Composition);
        m_primarySurface4Composition = NULL;
    }
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

    if( VA_INVALID_ID != m_detailFilterID )
    {
        vaDestroyBuffer(m_vaDisplay, m_detailFilterID);
    }

    if( VA_INVALID_ID != m_procampFilterID )
    {
        vaDestroyBuffer(m_vaDisplay, m_procampFilterID);
    }

    if( VA_INVALID_ID != m_deintFilterID )
    {
        vaDestroyBuffer(m_vaDisplay, m_deintFilterID);
    }

    if( VA_INVALID_ID != m_frcFilterID )
    {
        vaDestroyBuffer(m_vaDisplay, m_frcFilterID);
    }

    for(int i = 0; i < VAProcFilterCount; i++)
        m_filterBufs[i] = VA_INVALID_ID;

    m_denoiseFilterID   = VA_INVALID_ID;
    m_deintFilterID     = VA_INVALID_ID;
    m_procampFilterID   = VA_INVALID_ID;

    memset( (void*)&m_pipelineCaps, 0, sizeof(m_pipelineCaps));
    memset( (void*)&m_denoiseCaps, 0, sizeof(m_denoiseCaps));
    memset( (void*)&m_detailCaps, 0, sizeof(m_detailCaps));
    memset( (void*)&m_procampCaps,  0, sizeof(m_procampCaps));
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

    VAProcFilterType filters[VAProcFilterCount];
    mfxU32 num_filters = VAProcFilterCount;
    VAProcFilterCapFrameRateConversion tempFRC_Caps;
    mfxU32 num_frc_caps = 1;

    vaSts = vaQueryVideoProcFilters(m_vaDisplay, m_vaContextVPP, filters, &num_filters);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_procamp_caps = VAProcColorBalanceCount;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterColorBalance,
                               &m_procampCaps, &num_procamp_caps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_denoise_caps = 1;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterNoiseReduction,
                               &m_denoiseCaps, &num_denoise_caps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_detail_caps = 1;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterSharpening,
                               &m_detailCaps, &num_detail_caps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_deinterlacing_caps = VAProcDeinterlacingCount;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterDeinterlacing,
                               &m_deinterlacingCaps, &num_deinterlacing_caps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    /* to check is FRC enabled or not*/
    /* first need to get number of modes supported by driver*/
    tempFRC_Caps.bget_custom_rates = 1;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterFrameRateConversion,
                               &tempFRC_Caps, &num_frc_caps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if (0 != tempFRC_Caps.frc_custom_rates) /* FRC is enabled, at least one mode */
    {
        caps.uFrameRateConversion = 1 ;
        /* Again, only two modes: 24p->60p and 30p->60p is available
         * But driver report 3, but 3rd is equal to 2rd,
         * So only 2 real modes*/
        if (tempFRC_Caps.frc_custom_rates > 2)
            tempFRC_Caps.frc_custom_rates = 2;
        caps.frcCaps.customRateData.resize(tempFRC_Caps.frc_custom_rates);
        /*to get details about each mode */
        tempFRC_Caps.bget_custom_rates = 0;

        for (mfxU32 ii = 0; ii < tempFRC_Caps.frc_custom_rates; ii++)
        {
            m_frcCaps[ii].frc_custom_rates = ii + 1;
            vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                                                m_vaContextVPP,
                                                VAProcFilterFrameRateConversion,
                                                &m_frcCaps[ii], &num_frc_caps);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            caps.frcCaps.customRateData[ii].inputFramesOrFieldPerCycle = m_frcCaps[ii].input_frames;
            caps.frcCaps.customRateData[ii].outputIndexCountPerCycle = m_frcCaps[ii].output_frames;
            /* out frame rate*/
            caps.frcCaps.customRateData[ii].customRate.FrameRateExtN = m_frcCaps[ii].output_fps;
            /*input frame rate */
            caps.frcCaps.customRateData[ii].customRate.FrameRateExtD = m_frcCaps[ii].input_fps;
        }

    }

    for( mfxU32 filtersIndx = 0; filtersIndx < num_filters; filtersIndx++ )
    {
        if (filters[filtersIndx])
        {
            switch (filters[filtersIndx])
            {
                case VAProcFilterNoiseReduction:
                    caps.uDenoiseFilter = 1;
                    break;
                case VAProcFilterSharpening:
                    caps.uDetailFilter = 1;
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

    memset(&m_pipelineCaps,  0, sizeof(VAProcPipelineCaps));
    vaSts = vaQueryVideoProcPipelineCaps(m_vaDisplay,
                                 m_vaContextVPP,
                                 NULL,
                                 0,
                                 &m_pipelineCaps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    if (m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_90 ) &&
        m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_180) &&
        m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_270))
    {
        caps.uRotation = 1;
    }

    caps.uMaxWidth  = 4096;
    caps.uMaxHeight = 4096;

    caps.uFieldWeavingControl = 1;

    // [FourCC]
    // should be changed by libva support
    for (mfxU32 indx = 0; indx < sizeof(g_TABLE_SUPPORTED_FOURCC)/sizeof(mfxU32); indx++)
    {
        if (MFX_FOURCC_NV12 == g_TABLE_SUPPORTED_FOURCC[indx] ||
            MFX_FOURCC_YV12 == g_TABLE_SUPPORTED_FOURCC[indx] ||
            MFX_FOURCC_YUY2 == g_TABLE_SUPPORTED_FOURCC[indx] ||
            MFX_FOURCC_UYVY == g_TABLE_SUPPORTED_FOURCC[indx] ||
            MFX_FOURCC_RGB4 == g_TABLE_SUPPORTED_FOURCC[indx])
            caps.mFormatSupport[g_TABLE_SUPPORTED_FOURCC[indx]] |= MFX_FORMAT_SUPPORT_INPUT;

        if (MFX_FOURCC_NV12 == g_TABLE_SUPPORTED_FOURCC[indx] ||
            MFX_FOURCC_RGB4 == g_TABLE_SUPPORTED_FOURCC[indx])
            caps.mFormatSupport[g_TABLE_SUPPORTED_FOURCC[indx]] |= MFX_FORMAT_SUPPORT_OUTPUT;
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::QueryCapabilities(mfxVppCaps& caps)

mfxStatus VAAPIVideoProcessing::Execute(mfxExecuteParams *pParams)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "VPP Execute");

    VASurfaceAttrib attrib;
    VAImage imagePrimarySurface;
    mfxU8* pPrimarySurfaceBuffer;
    VAImage imageRefSurface;
    mfxU8* pRefSurfaceBuffer;
    bool   bForceADI = false;

    VAStatus vaSts = VA_STATUS_SUCCESS;

    MFX_CHECK_NULL_PTR1( pParams );
    MFX_CHECK_NULL_PTR1( pParams->targetSurface.hdl.first );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces[0].hdl.first );

    /* There is a special case for composition */
    mfxStatus mfxSts = MFX_ERR_NONE;
    if (pParams->bComposite)
    {
        mfxSts = Execute_Composition(pParams);
        return mfxSts;
    }

    /* Now msdk needs intermediate surface for ADI with ref 30i->30p mode*/
    if ((m_primarySurface4Composition == NULL) && (pParams->iDeinterlacingAlgorithm !=0) )
    {
        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[0]);
        mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);

        m_primarySurface4Composition = (VASurfaceID*)calloc(1,sizeof(VASurfaceID));
        /* KW fix, but it is true, required to check, is memory allocated o not  */
        if (m_primarySurface4Composition == NULL)
            return MFX_ERR_MEMORY_ALLOC;

        attrib.type = VASurfaceAttribPixelFormat;
        attrib.value.type = VAGenericValueTypeInteger;

        // default format is NV12
        if (inInfo->FourCC == MFX_FOURCC_RGB4)
        {
            attrib.value.value.i = VA_FOURCC_ARGB;
        }
        else
        {
            attrib.value.value.i = VA_FOURCC_NV12;
        }
        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;

        vaSts = vaCreateSurfaces(m_vaDisplay, attrib.value.value.i, inInfo->Width, inInfo->Height,
                m_primarySurface4Composition, 1, &attrib, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    if (VA_INVALID_ID == m_deintFilterID)
    {
        if (pParams->iDeinterlacingAlgorithm)
        {
            VAProcFilterParameterBufferDeinterlacing deint;
            deint.type  = VAProcFilterDeinterlacing;
            deint.flags = 0;

            if (MFX_DEINTERLACING_BOB == pParams->iDeinterlacingAlgorithm)
            {
                deint.algorithm = VAProcDeinterlacingBob;
            }
            else
            {
                if (pParams->bEOS)
                {
                    // WA for the last frame
                    // otherwise it's not processed properly
                    bForceADI = true;
                }
                deint.algorithm = VAProcDeinterlacingMotionAdaptive;
            }

            mfxDrvSurface* pRefSurf_frameInfo = &(pParams->pRefSurfaces[0]);
            switch (pRefSurf_frameInfo->frameInfo.PicStruct)
            {
                case MFX_PICSTRUCT_PROGRESSIVE:
                    deint.flags = VA_DEINTERLACING_ONE_FIELD;
                    break;

                case MFX_PICSTRUCT_FIELD_TFF:
                    break; // don't set anything, see comment in va_vpp.h

                case MFX_PICSTRUCT_FIELD_BFF:
                    deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST | VA_DEINTERLACING_BOTTOM_FIELD;
                    break;
            }
            /* For ADI 30i->60p case with reference frames we have to indicate
             * to driver which field Top or Bottom
             * we a going to send now. Or which filed will be used by driver from source
             * (not from reference) for current iteration.
             * So on, if APP set "-spic 0" which is means MFX_PICSTRUCT_FIELD_TFF, in second iteration
             * we should set "deint.flags = VA_DEINTERLACING_BOTTOM_FIELD" to indicate what now we processing
             * Bottom field at this moment. Third iteration again TFF... and etc.
             * if APP set "-spic 2" which is means MFX_PICSTRUCT_FIELD_BFF all is vice versa.
             *  */
            if (((pParams->bFMDEnable == true) && (pParams->refCount > 1)) || bForceADI)
            {
                if (0 == (m_refCountForADI%2) )
                {
                    if (MFX_PICSTRUCT_FIELD_TFF == pRefSurf_frameInfo->frameInfo.PicStruct)
                        deint.flags = 0;
                    else /**/
                        deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST | VA_DEINTERLACING_BOTTOM_FIELD;
                }
                else
                {
                    if (MFX_PICSTRUCT_FIELD_TFF == pRefSurf_frameInfo->frameInfo.PicStruct)
                        deint.flags = VA_DEINTERLACING_BOTTOM_FIELD;
                    else /**/
                        deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST;
                }
            } //  if ((pParams->bFMDEnable == true) && (pParams->refCount > 1)) /* For 30i->60p mode only*/

            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextVPP,
                                   VAProcFilterParameterBufferType,
                                   sizeof(deint), 1,
                                   &deint, &m_deintFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_deintFilterID;
        }
    }

    if (VA_INVALID_ID == m_procampFilterID)
    {
        if ( pParams->bEnableProcAmp )
        {
            VAProcFilterParameterBufferColorBalance procamp[4];

            procamp[0].type   = VAProcFilterColorBalance;
            procamp[0].attrib = VAProcColorBalanceBrightness;
            procamp[0].value  = 0;

            if ( pParams->Brightness)
            {
                procamp[0].value = convertValue(-100,
                                100,
                                m_procampCaps[VAProcColorBalanceBrightness-1].range.min_value,
                                m_procampCaps[VAProcColorBalanceBrightness-1].range.max_value,
                                pParams->Brightness);
            }

            procamp[1].type   = VAProcFilterColorBalance;
            procamp[1].attrib = VAProcColorBalanceContrast;
            procamp[1].value  = 1;

            if ( pParams->Contrast != 1 )
            {
                procamp[1].value = convertValue(0,
                                10,
                                m_procampCaps[VAProcColorBalanceContrast-1].range.min_value,
                                m_procampCaps[VAProcColorBalanceContrast-1].range.max_value,
                                pParams->Contrast);
            }

            procamp[2].type   = VAProcFilterColorBalance;
            procamp[2].attrib = VAProcColorBalanceHue;
            procamp[2].value  = 0;

            if ( pParams->Hue)
            {
                procamp[2].value = convertValue(-180,
                                180,
                                m_procampCaps[VAProcColorBalanceHue-1].range.min_value,
                                m_procampCaps[VAProcColorBalanceHue-1].range.max_value,
                                pParams->Hue);
            }

            procamp[3].type   = VAProcFilterColorBalance;
            procamp[3].attrib = VAProcColorBalanceSaturation;
            procamp[3].value  = 1;

            if ( pParams->Saturation != 1 )
            {
                procamp[3].value = convertValue(0,
                                10,
                                m_procampCaps[VAProcColorBalanceSaturation-1].range.min_value,
                                m_procampCaps[VAProcColorBalanceSaturation-1].range.max_value,
                                pParams->Saturation);
            }

            vaSts = vaCreateBuffer((void*)m_vaDisplay,
                                          m_vaContextVPP,
                                          VAProcFilterParameterBufferType,
                                          sizeof(procamp), 4,
                                          &procamp, &m_procampFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_procampFilterID;
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
                denoise.value = (mfxU16)floor(
                                            convertValue(0,
                                              100,
                                              m_denoiseCaps.range.min_value,
                                              m_denoiseCaps.range.max_value,
                                              pParams->denoiseFactor) + 0.5);
            vaSts = vaCreateBuffer((void*)m_vaDisplay,
                          m_vaContextVPP,
                          VAProcFilterParameterBufferType,
                          sizeof(denoise), 1,
                          &denoise, &m_denoiseFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_denoiseFilterID;
        }
    }

    if (VA_INVALID_ID == m_detailFilterID)
    {
        if (pParams->detailFactor || true == pParams->bDetailAutoAdjust)
        {
            VAProcFilterParameterBuffer detail;
            detail.type  = VAProcFilterSharpening;
            if(pParams->bDetailAutoAdjust)
                detail.value = m_detailCaps.range.default_value;
            else
                detail.value = (mfxU16)floor(
                                           convertValue(0,
                                             100,
                                             m_detailCaps.range.min_value,
                                             m_detailCaps.range.max_value,
                                             pParams->detailFactor) + 0.5);
            vaSts = vaCreateBuffer((void*)m_vaDisplay,
                          m_vaContextVPP,
                          VAProcFilterParameterBufferType,
                          sizeof(detail), 1,
                          &detail, &m_detailFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_detailFilterID;
        }
    }

    if (VA_INVALID_ID == m_frcFilterID)
    {
        if (pParams->bFRCEnable)
        {
          VAProcFilterParameterBufferFrameRateConversion frcParams;
          /* Actually KW fix */
          memset(&frcParams, 0, sizeof(VAProcFilterParameterBufferFrameRateConversion));
          frcParams.type = VAProcFilterFrameRateConversion;
          if (30 == pParams->customRateData.customRate.FrameRateExtD)
          {
              frcParams.input_fps = 30;
              frcParams.output_fps = 60;
              frcParams.num_output_frames = 2;
              frcParams.repeat_frame = 0;
          }
          else if (24 == pParams->customRateData.customRate.FrameRateExtD)
          {
              frcParams.input_fps = 24;
              frcParams.output_fps = 60;
              frcParams.num_output_frames = 5;
              frcParams.repeat_frame = 0;
          }

          frcParams.cyclic_counter = m_frcCyclicCounter++;
          if ((frcParams.input_fps == 30) && (m_frcCyclicCounter == 2))
                m_frcCyclicCounter = 0;
          if ((frcParams.input_fps == 24) && (m_frcCyclicCounter == 5))
                m_frcCyclicCounter = 0;
          frcParams.output_frames = (VASurfaceID*)(pParams->targetSurface.hdl.first);
          vaSts = vaCreateBuffer((void*)m_vaDisplay,
                        m_vaContextVPP,
                        VAProcFilterParameterBufferType,
                        sizeof(frcParams), 1,
                        &frcParams, &m_frcFilterID);
          MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

          m_filterBufs[m_numFilterBufs++] = m_frcFilterID;
        }
    }

    /* pParams->refCount is total number of processing surfaces:
     * in case of composition this is primary + sub streams*/

    // mfxU32 SampleCount = pParams->refCount;
    // WA:
    mfxU32 SampleCount = 1;
    mfxU32 refIdx = 0;

    //m_pipelineParam.resize(SampleCount);
    //m_pipelineParam.clear();
    m_pipelineParam.resize(pParams->refCount);
    //m_pipelineParamID.resize(SampleCount);
    //m_pipelineParamID.clear();
    m_pipelineParamID.resize(pParams->refCount, VA_INVALID_ID);

    std::vector<VARectangle> input_region;
    input_region.resize(pParams->refCount);
    std::vector<VARectangle> output_region;
    output_region.resize(pParams->refCount);

    for( refIdx = 0; refIdx < SampleCount; refIdx++ )
    {
        mfxDrvSurface* pRefSurf;
        pRefSurf = &(pParams->pRefSurfaces[refIdx]);

        if (bForceADI || ( (pParams->refCount > 1) && (0 != pParams->iDeinterlacingAlgorithm )))
        {
            m_refCountForADI++;
            m_pipelineParam[refIdx].num_backward_references = 1;
            mfxDrvSurface* pRefSurf_1 = NULL;
            /* in pRefSurfaces
             * first is backward references
             * then current src surface
             * and only after this is forward references
             * */
            pRefSurf_1 = &(pParams->pRefSurfaces[0]);
            VASurfaceID* ref_srf = (VASurfaceID*) (pRefSurf_1->hdl.first);
            m_pipelineParam[refIdx].backward_references = ref_srf;
        }
        /* FRC Interpolated case */
        if (0 != pParams->bFRCEnable)
        {
            if (30 == pParams->customRateData.customRate.FrameRateExtD)
                m_pipelineParam[refIdx].num_forward_references = 2;
            else if (24 == pParams->customRateData.customRate.FrameRateExtD)
                m_pipelineParam[refIdx].num_forward_references = 3;

            if (2 == pParams->refCount) /* may be End of Stream case */
            {
                mfxDrvSurface* pRefSurf_frc1;
                pRefSurf_frc1 = &(pParams->pRefSurfaces[1]);
                m_refForFRC[0] = *(VASurfaceID*)(pRefSurf_frc1->hdl.first);
                m_refForFRC[2] = m_refForFRC[1] = m_refForFRC[0];
            }
            if (3 == pParams->refCount)
            {
                mfxDrvSurface* pRefSurf_frc1;
                pRefSurf_frc1 = &(pParams->pRefSurfaces[1]);
                m_refForFRC[0] = *(VASurfaceID*)(pRefSurf_frc1->hdl.first);
                mfxDrvSurface* pRefSurf_frc2;
                pRefSurf_frc2 = &(pParams->pRefSurfaces[2]);
                m_refForFRC[1] = *(VASurfaceID*) (pRefSurf_frc2->hdl.first);
            }
            if (4 == pParams->refCount)
            {
                mfxDrvSurface* pRefSurf_frc1;
                pRefSurf_frc1 = &(pParams->pRefSurfaces[1]);
                m_refForFRC[0] = *(VASurfaceID*)(pRefSurf_frc1->hdl.first);
                mfxDrvSurface* pRefSurf_frc2;
                pRefSurf_frc2 = &(pParams->pRefSurfaces[2]);
                m_refForFRC[1] = *(VASurfaceID*) (pRefSurf_frc2->hdl.first);
                mfxDrvSurface* pRefSurf_frc3;
                pRefSurf_frc3 = &(pParams->pRefSurfaces[3]);
                m_refForFRC[2] = *(VASurfaceID*) (pRefSurf_frc3->hdl.first);
            }
            /* to pass ref list to pipeline */
            m_pipelineParam[refIdx].forward_references = m_refForFRC;
        } /*if (0 != pParams->bFRCEnable)*/

        /* SRC surface */
        mfxDrvSurface* pSrcInputSurf = &pRefSurf[pParams->bkwdRefCount];
        VASurfaceID* srf = (VASurfaceID*)(pSrcInputSurf->hdl.first);
        m_pipelineParam[refIdx].surface = *srf;

        switch (pParams->rotation)
        {
        case MFX_ANGLE_90:
            if ( m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_90))
            {
                m_pipelineParam[refIdx].rotation_state = VA_ROTATION_90;
            }
            break;
        case MFX_ANGLE_180:
            if ( m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_180))
            {
                m_pipelineParam[refIdx].rotation_state = VA_ROTATION_180;
            }
            break;
        case MFX_ANGLE_270:
            if ( m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_270))
            {
                m_pipelineParam[refIdx].rotation_state = VA_ROTATION_270;
            }
            break;
        }

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

        m_pipelineParam[refIdx].output_background_color = 0xff000000; // black for ARGB

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

    MFX_LTRACE_2(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "A|VPP|FILTER|PACKET_START|", "%d|%d", m_vaContextVPP, 0);
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
        for( refIdx = 0; refIdx < SampleCount; refIdx++ )
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
    MFX_LTRACE_2(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "A|VPP|FILTER|PACKET_END|", "%d|%d", m_vaContextVPP, 0);

    for( refIdx = 0; refIdx < m_pipelineParamID.size(); refIdx++ )
    {
        if ( m_pipelineParamID[refIdx] != VA_INVALID_ID)
        {
            vaSts = vaDestroyBuffer(m_vaDisplay, m_pipelineParamID[refIdx]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            m_pipelineParamID[refIdx] = VA_INVALID_ID;
        }
    }

    if (m_deintFilterID != VA_INVALID_ID)
    {
        vaSts = vaDestroyBuffer(m_vaDisplay, m_deintFilterID);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        std::remove(m_filterBufs, m_filterBufs + m_numFilterBufs, m_deintFilterID);
        m_deintFilterID = VA_INVALID_ID;
        m_numFilterBufs--;
        m_filterBufs[m_numFilterBufs] = VA_INVALID_ID;
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


    if (m_frcFilterID != VA_INVALID_ID)
    {
        vaSts = vaDestroyBuffer(m_vaDisplay, m_frcFilterID);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        std::remove(m_filterBufs, m_filterBufs + m_numFilterBufs, m_frcFilterID);
        m_frcFilterID = VA_INVALID_ID;
        m_filterBufs[m_numFilterBufs] = VA_INVALID_ID;
        m_numFilterBufs--;
    }

    /* drop "internal" ADI frame for 30i->30p mode
     * HW or EU kernels always generates from interlaced SRC and REF frames two
     * de-interlaced frames, so by fact HW is always working in 30i->60p mode.
     * But in case of 30i->30p these two frames will be the same. Driver return one resulted
     * frame immediately, and second frame for next vaBeginPicture() ... vaEndPicture() call.
     * Without real calculation of fames according new params. This is ok for 30i->60p mode but
     * for 30i->30p mode application needs to drop this is second internal frame.
     * */

//    if ((pParams->refCount > 1) && (pParams->bFMDEnable == false) && (0 != pParams->iDeinterlacingAlgorithm))
//    {
//        vaSts = vaSyncSurface(m_vaDisplay,*outputSurface);
//        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//
//        m_bFakeOutputEnabled = true;
//
//        mfxStatus mfxSts = Execute_FakeOutput(pParams);
//
//        m_bFakeOutputEnabled = false;
//        if (mfxSts != MFX_ERR_NONE)
//            return mfxSts;
//    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Execute(FASTCOMP_BLT_PARAMS *pVideoCompositingBlt)

mfxStatus VAAPIVideoProcessing::Execute_FakeOutput(mfxExecuteParams *pParams)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "VPP Execute");

    VASurfaceAttrib attrib;
    VAImage imagePrimarySurface;
    mfxU8* pPrimarySurfaceBuffer;
    VAImage imageRefSurface;
    mfxU8* pRefSurfaceBuffer;

    VAStatus vaSts = VA_STATUS_SUCCESS;

    MFX_CHECK_NULL_PTR1( pParams );
    MFX_CHECK_NULL_PTR1( pParams->targetSurface.hdl.first );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces[0].hdl.first );

    /* There is a special case for composition */
    mfxStatus mfxSts = MFX_ERR_NONE;
    if (pParams->bComposite)
    {
        mfxSts = Execute_Composition(pParams);
        return mfxSts;
    }

    if (VA_INVALID_ID == m_deintFilterID)
    {
        if (pParams->iDeinterlacingAlgorithm)
        {
            VAProcFilterParameterBufferDeinterlacing deint;
            deint.type  = VAProcFilterDeinterlacing;
            deint.flags = 0;
            //WA for VPG driver. Need to rewrite it with caps usage when driver begins to return a correct list of supported DI algorithms
#ifndef MFX_VA_ANDROID
            if (MFX_DEINTERLACING_BOB == pParams->iDeinterlacingAlgorithm)
            {
                deint.algorithm = VAProcDeinterlacingBob;
            }
            else
            {
                deint.algorithm = VAProcDeinterlacingMotionAdaptive;
            }

            mfxDrvSurface* pRefSurf_frameInfo = &(pParams->pRefSurfaces[0]);
            switch (pRefSurf_frameInfo->frameInfo.PicStruct)
            {
                case MFX_PICSTRUCT_PROGRESSIVE:
                    deint.flags = VA_DEINTERLACING_ONE_FIELD;
                    break;

                case MFX_PICSTRUCT_FIELD_TFF:
                    break; // don't set anything, see comment in va_vpp.h

                case MFX_PICSTRUCT_FIELD_BFF:
                    deint.flags = VA_DEINTERLACING_BOTTOM_FIELD;
                    break;
            }
            /* For ADI with reference frames we have to indicate to driver which field  Top or Bottom
             * we a going to send now. Or which filed will be used by driver from source
             * (not from reference) for current iteration.
             * So on, if APP set "-spic 0" which is means MFX_PICSTRUCT_FIELD_TFF, in second iteration
             * we should set "deint.flags = VA_DEINTERLACING_BOTTOM_FIELD" to indicate what now we processing
             * Bottom field at this moment. Third iteration again TFF... and etc.
             * if APP set "-spic 2" which is means MFX_PICSTRUCT_FIELD_BFF all is vice versa.
             *  */
            if (pParams->bFMDEnable == true) /* For 30i->60p mode only*/
            {
                if ( (pParams->refCount > 1) && (((m_refCountForADI+1))%2 == 1) )
                {
                    if (deint.flags == 0)
                        deint.flags = VA_DEINTERLACING_BOTTOM_FIELD;
                    else if (deint.flags == VA_DEINTERLACING_BOTTOM_FIELD)
                        deint.flags = 0;
                }
            }

            if ( (pParams->bFMDEnable == false) && /*For 30i->30p only, but not for 30i->60p! */
                 (pParams->refCount > 1) && /* ADI with references and ... */
                 (pRefSurf_frameInfo->frameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF)) /* for TFF*/
            {
                deint.flags = VA_DEINTERLACING_BOTTOM_FIELD;
            }

            /* Default properties is TFF,
             * so if you want BFF you need to change it */
            if ( (pParams->bFMDEnable == false) && /* For 30i->30p only, but not for 30i->60p! */
                 (pParams->refCount > 1) && /* ADI with references and ... */
                 (pRefSurf_frameInfo->frameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)) /* for BFF*/
            {
                deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST;
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

    /* pParams->refCount is total number of processing surfaces:
     * in case of composition this is primary + sub streams*/

    //mfxU32 SampleCount = pParams->refCount;
    // WA:
    mfxU32 SampleCount = 1;
    mfxU32 refIdx = 0;

    m_pipelineParam.resize(pParams->refCount);
    m_pipelineParamID.resize(pParams->refCount, VA_INVALID_ID);

    std::vector<VARectangle> input_region;
    input_region.resize(pParams->refCount);
    std::vector<VARectangle> output_region;
    output_region.resize(pParams->refCount);

    for( refIdx = 0; refIdx < SampleCount; refIdx++ )
    {
        mfxDrvSurface* pRefSurf;
        pRefSurf = &(pParams->pRefSurfaces[refIdx]);

        if (pParams->refCount > 1)
        {
            m_pipelineParam[refIdx].num_backward_references = 1;
            mfxDrvSurface* pRefSurf_1;
            pRefSurf_1 = &(pParams->pRefSurfaces[1]);
            VASurfaceID* ref_srf = (VASurfaceID*) (pRefSurf_1->hdl.first);
            m_pipelineParam[refIdx].backward_references = ref_srf;
        }

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

        m_pipelineParam[refIdx].output_background_color = 0xff000000; // black for ARGB

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

    }// for( refIdx = 0; refIdx < SampleCount; refIdx++ )

    MFX_LTRACE_2(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "A|VPP|SKIP|PACKET_START|", "%d|%d", m_vaContextVPP, 0);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
        vaSts = vaBeginPicture(m_vaDisplay,
                            m_vaContextVPP,
                            *m_primarySurface4Composition);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
        for( refIdx = 0; refIdx < SampleCount; refIdx++ )
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
    MFX_LTRACE_2(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "A|VPP|SKIP|PACKET_END|", "%d|%d", m_vaContextVPP, 0);

    for( refIdx = 0; refIdx < m_pipelineParamID.size(); refIdx++ )
    {
        if ( m_pipelineParamID[refIdx] != VA_INVALID_ID)
        {
            vaSts = vaDestroyBuffer(m_vaDisplay, m_pipelineParamID[refIdx]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            m_pipelineParamID[refIdx] = VA_INVALID_ID;
        }
    }

    if (m_deintFilterID != VA_INVALID_ID)
    {
        vaSts = vaDestroyBuffer(m_vaDisplay, m_deintFilterID);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        std::remove(m_filterBufs, m_filterBufs + m_numFilterBufs, m_deintFilterID);
        m_deintFilterID = VA_INVALID_ID;
        m_numFilterBufs--;
        m_filterBufs[m_numFilterBufs] = VA_INVALID_ID;
    }

    return MFX_ERR_NONE;
}


mfxStatus VAAPIVideoProcessing::Execute_Composition(mfxExecuteParams *pParams)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "VPP Execute");

    VAStatus vaSts = VA_STATUS_SUCCESS;
    VASurfaceAttrib attrib;
    std::vector<VABlendState> blend_state;
    VAImage imagePrimarySurface;
    mfxU8* pPrimarySurfaceBuffer;

    MFX_CHECK_NULL_PTR1( pParams );
    MFX_CHECK_NULL_PTR1( pParams->targetSurface.hdl.first );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces[0].hdl.first );

    if (m_primarySurface4Composition == NULL)
    {
        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[0]);
        mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);

        m_primarySurface4Composition = (VASurfaceID*)calloc(1,1*sizeof(VASurfaceID));
        /* KW fix, but it is true, required to check, is memory allocated o not  */
        if (m_primarySurface4Composition == NULL)
            return MFX_ERR_MEMORY_ALLOC;

        attrib.type = VASurfaceAttribPixelFormat;
        attrib.value.type = VAGenericValueTypeInteger;

        // default format is NV12
        if (inInfo->FourCC == MFX_FOURCC_RGB4)
        {
            attrib.value.value.i = VA_FOURCC_ARGB;
        }
        else
        {
            attrib.value.value.i = VA_FOURCC_NV12;
        }
        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;

        vaSts = vaCreateSurfaces(m_vaDisplay, attrib.value.value.i, inInfo->Width, inInfo->Height,
                m_primarySurface4Composition, 1, &attrib, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        for ( int iPrSurfCount = 0; iPrSurfCount < 1; iPrSurfCount++)
        {
            vaSts = vaDeriveImage(m_vaDisplay, m_primarySurface4Composition[iPrSurfCount], &imagePrimarySurface);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            vaSts = vaMapBuffer(m_vaDisplay, imagePrimarySurface.buf, (void **) &pPrimarySurfaceBuffer);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            IppStatus ippSts = ippStsNoErr;
            IppiSize roiSize;
            roiSize.width = imagePrimarySurface.width;
            roiSize.height = imagePrimarySurface.height;

            /* We need to fill up empty surface by background color...
             * it is easy for ARGB format as Initial background value ARGB*/
            if (imagePrimarySurface.format.fourcc == VA_FOURCC_ARGB)
            {
                const Ipp8u backgroundValues[4] = { (Ipp8u)((pParams->iBackgroundColor &0x000000ff) >> 0),
                                                    (Ipp8u)((pParams->iBackgroundColor &0x0000ff00) >> 8),
                                                    (Ipp8u)((pParams->iBackgroundColor &0x00ff0000) >>16),
                                                    (Ipp8u)((pParams->iBackgroundColor &0xff000000) >>24)};
                ippSts = ippiSet_8u_C4R(backgroundValues, pPrimarySurfaceBuffer,
                                    imagePrimarySurface.pitches[0], roiSize);
                MFX_CHECK(ippStsNoErr == ippSts, MFX_ERR_DEVICE_FAILED);
            }
            /* A bit more complicated for NV12 as you need to do conversion ARGB => NV12 */
            if (imagePrimarySurface.format.fourcc == VA_FOURCC_NV12)
            {
                Ipp32u Y = (Ipp32u)((pParams->iBackgroundColor &0x00ff0000) >>16);
                Ipp32u U = (Ipp32u)((pParams->iBackgroundColor &0x0000ff00) >>8);
                Ipp32u V = (Ipp32u)((pParams->iBackgroundColor &0x000000ff) );

                Ipp8u valueY = (Ipp8u) Y;
                Ipp16s valueUV = (Ipp16s)((U<<8)  + V);

                ippSts = ippiSet_8u_C1R(valueY, pPrimarySurfaceBuffer, imagePrimarySurface.pitches[0], roiSize);
                MFX_CHECK(ippStsNoErr == ippSts, MFX_ERR_DEVICE_FAILED);
                //
                roiSize.height = roiSize.height/2;
                ippSts = ippiSet_16s_C1R(valueUV, (Ipp16s *)(pPrimarySurfaceBuffer + imagePrimarySurface.offsets[1]),
                                                imagePrimarySurface.pitches[1], roiSize);
                MFX_CHECK(ippStsNoErr == ippSts, MFX_ERR_DEVICE_FAILED);
            }

            vaSts = vaUnmapBuffer(m_vaDisplay, imagePrimarySurface.buf);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            vaSts = vaDestroyImage(m_vaDisplay, imagePrimarySurface.image_id);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        } // for ( int iPrSurfCount = 0; iPrSurfCount < 3; iPrSurfCount++)
    }

    /* pParams->refCount is total number of processing surfaces:
     * in case of composition this is primary + sub streams*/

    // mfxU32 SampleCount = pParams->refCount;
    // WA:
    mfxU32 SampleCount = 1;
    mfxU32 refIdx = 0;
    mfxU32 refCount = (mfxU32) pParams->fwdRefCount;

    //m_pipelineParam.resize(SampleCount);
    //m_pipelineParam.clear();
    m_pipelineParam.resize(pParams->refCount + 1);
    //m_pipelineParamID.resize(SampleCount);
    //m_pipelineParamID.clear();
    m_pipelineParamID.resize(pParams->refCount + 1, VA_INVALID_ID);
    blend_state.resize(pParams->refCount + 1);

    std::vector<VARectangle> input_region;
    input_region.resize(pParams->refCount + 1);
    std::vector<VARectangle> output_region;
    output_region.resize(pParams->refCount + 1);
    VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);

    for( refIdx = 0; refIdx < SampleCount; refIdx++ )
    {
        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[refIdx]);

        //VASurfaceID* srf_1 = (VASurfaceID*)(pRefSurf->hdl.first);
        //m_pipelineParam[refIdx].surface = *srf_1;
        /* First "primary" surface should be our allocated empty surface filled by background color.
         * Else we can not process first input surface as usual one */
        m_pipelineParam[refIdx].surface = m_primarySurface4Composition[0];
        //VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);
        //m_pipelineParam[refIdx].surface = *outputSurface;

        // source cropping
        //mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
        mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);
        input_region[refIdx].y   = 0;
        input_region[refIdx].x   = 0;
        input_region[refIdx].height = outInfo->CropH;
        input_region[refIdx].width  = outInfo->CropW;
        m_pipelineParam[refIdx].surface_region = &input_region[refIdx];

        // destination cropping
        //mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);
        output_region[refIdx].y  = 0; //outInfo->CropY;
        output_region[refIdx].x   = 0; //outInfo->CropX;
        output_region[refIdx].height= outInfo->CropH;
        output_region[refIdx].width  = outInfo->CropW;
        m_pipelineParam[refIdx].output_region = &output_region[refIdx];

        /* Actually as background color managed by "m_primarySurface4Composition" surface
         * this param will not make sense */
        //m_pipelineParam[refIdx].output_background_color = pParams->iBackgroundColor;

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
            default:
                m_pipelineParam[refIdx].filter_flags = VA_FRAME_PICTURE;
                break;
#if 0
            // Composition always expect frames to be marked as progressive even if they are interlaced
            case MFX_PICSTRUCT_FIELD_TFF:
                m_pipelineParam[refIdx].filter_flags = VA_TOP_FIELD;
                break;
            case MFX_PICSTRUCT_FIELD_BFF:
                m_pipelineParam[refIdx].filter_flags = VA_BOTTOM_FIELD;
                break;
#endif
        }

        m_pipelineParam[refIdx].filters  = m_filterBufs;
        m_pipelineParam[refIdx].num_filters  = m_numFilterBufs;
        /* Special case for composition:
         * as primary surface processed as sub-stream
         * pipeline and filter properties should be *_FAST */
        if (pParams->bComposite)
        {
            m_pipelineParam[refIdx].num_filters  = 0;
#ifndef LINUX_TARGET_PLATFORM_BSW
            m_pipelineParam[refIdx].pipeline_flags |= VA_PROC_PIPELINE_FAST;
            //m_pipelineParam[refIdx].filter_flags    |= VA_FILTER_SCALING_FAST;
#else
            m_pipelineParam[refIdx].pipeline_flags |= VA_PROC_PIPELINE_SUBPICTURES;
            m_pipelineParam[refIdx].filter_flags    |= VA_FILTER_SCALING_HQ;
#endif
        }

        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextVPP,
                            VAProcPipelineParameterBufferType,
                            sizeof(VAProcPipelineParameterBuffer),
                            1,
                            &m_pipelineParam[refIdx],
                            &m_pipelineParamID[refIdx]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    MFX_LTRACE_2(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "A|VPP|COMP|PACKET_START|", "%d|%d", m_vaContextVPP, 0);
    //VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);
    //if ((refCount + 1) < 8)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
        vaSts = vaBeginPicture(m_vaDisplay,
                            m_vaContextVPP,
                            *outputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
//    else
//    {
//        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
//        vaSts = vaBeginPicture(m_vaDisplay,
//                            m_vaContextVPP,
//                            m_primarySurface4Composition[1]);
//        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
        for( refIdx = 0; refIdx < SampleCount; refIdx++ )
        {
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[refIdx], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    unsigned int uBeginPictureCounter = 0;
    //unsigned int uInputIndex = 1, uOutputIndex = 2;
    std::vector<VAProcPipelineParameterBuffer> m_pipelineParamComp;
    std::vector<VABufferID> m_pipelineParamCompID;
    /* for new buffers for Begin Picture*/
    m_pipelineParamComp.resize(pParams->fwdRefCount/7);
    m_pipelineParamCompID.resize(pParams->fwdRefCount/7, VA_INVALID_ID);

    /* pParams->fwdRefCount actually is number of sub stream*/
    for( refIdx = 1; refIdx <= (refCount + 1); refIdx++ )
    //for( refIdx = uStartIndex; refIdx <= uEndIndex; refIdx++ )
    {
        /*for frames 8, 15, 22, 29,... */
        unsigned int uLastPass = (refCount + 1) - ( (refIdx /7) *7);
        if ((refIdx != 1) && ((refIdx %7) == 1) )
        {
            //m_pipelineParam[refIdx].output_background_color = pParams->iBackgroundColor;
            //if ((uLastPass < 8) && (refIdx != 1))
            {
                vaSts = vaBeginPicture(m_vaDisplay,
                                    m_vaContextVPP,
                                    *outputSurface);
            }
            //else
            //{
            //vaSts = vaBeginPicture(m_vaDisplay,
            //                    m_vaContextVPP,
            //                    m_primarySurface4Composition[uOutputIndex]);
            //}
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            /*to copy initial properties of primary surface... */
            m_pipelineParamComp[uBeginPictureCounter] = m_pipelineParam[0];
            /* ... and to In-place output*/
            //m_pipelineParamComp[uBeginPictureCounter].surface = m_primarySurface4Composition[uInputIndex];
            m_pipelineParamComp[uBeginPictureCounter].surface = *outputSurface;
            //m_pipelineParam[0].surface = *outputSurface;
            /* As used IN-PLACE variant of Composition
             * this values does not used*/
            //uOutputIndex++;
            //uInputIndex++;
            //if (uOutputIndex > 2)
            //    uOutputIndex = 0;
            //if (uInputIndex > 2)
            //    uInputIndex = 0;

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextVPP,
                                VAProcPipelineParameterBufferType,
                                sizeof(VAProcPipelineParameterBuffer),
                                1,
                                &m_pipelineParamComp[uBeginPictureCounter],
                                &m_pipelineParamCompID[uBeginPictureCounter]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamCompID[uBeginPictureCounter], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            uBeginPictureCounter++;
        }

        m_pipelineParam[refIdx] = m_pipelineParam[0];

        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[refIdx-1]);

        VASurfaceID* srf_2 = (VASurfaceID*)(pRefSurf->hdl.first);

        m_pipelineParam[refIdx].surface = *srf_2;

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

        /* to process input parameters of sub stream:
         * crop info and original size*/
        mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
        input_region[refIdx].y   = inInfo->CropY;
        input_region[refIdx].x   = inInfo->CropX;
        input_region[refIdx].height = inInfo->CropH;
        input_region[refIdx].width  = inInfo->CropW;
        m_pipelineParam[refIdx].surface_region = &input_region[refIdx];

        /* to process output parameters of sub stream:
         *  position and destination size */
        output_region[refIdx].y  = pParams->dstRects[refIdx-1].DstY;
        output_region[refIdx].x   = pParams->dstRects[refIdx-1].DstX;
        output_region[refIdx].height= pParams->dstRects[refIdx-1].DstH;
        output_region[refIdx].width  = pParams->dstRects[refIdx-1].DstW;
        m_pipelineParam[refIdx].output_region = &output_region[refIdx];

        /* Global alpha and luma key can not be enabled together*/
        /* Global alpha and luma key can not be enabled together*/
        if (pParams->dstRects[refIdx-1].GlobalAlphaEnable !=0)
        {
            blend_state[refIdx].flags = VA_BLEND_GLOBAL_ALPHA;
            blend_state[refIdx].global_alpha = ((float)pParams->dstRects[refIdx-1].GlobalAlpha) /255;
        }
        /* Luma color key  for YUV surfaces only.
         * And Premultiplied alpha blending for RGBA surfaces only.
         * So, these two flags can't combine together  */
        if ((pParams->dstRects[refIdx-1].LumaKeyEnable != 0) &&
            (pParams->dstRects[refIdx-1].PixelAlphaEnable == 0) )
        {
            blend_state[refIdx].flags |= VA_BLEND_LUMA_KEY;
            blend_state[refIdx].min_luma = pParams->dstRects[refIdx-1].LumaKeyMin;
            blend_state[refIdx].max_luma = pParams->dstRects[refIdx-1].LumaKeyMax;
        }
        if ((pParams->dstRects[refIdx-1].LumaKeyEnable == 0 ) &&
            (pParams->dstRects[refIdx-1].PixelAlphaEnable != 0 ) )
        {
            blend_state[refIdx].flags |= VA_BLEND_PREMULTIPLIED_ALPHA;
        }
        if ((pParams->dstRects[refIdx-1].GlobalAlphaEnable != 0) ||
                (pParams->dstRects[refIdx-1].LumaKeyEnable != 0) ||
                (pParams->dstRects[refIdx-1].PixelAlphaEnable != 0))
        {
            m_pipelineParam[refIdx].blend_state = &blend_state[refIdx];
        }

        //m_pipelineParam[refIdx].pipeline_flags = ?? //VA_PROC_PIPELINE_FAST or VA_PROC_PIPELINE_SUBPICTURES
        m_pipelineParam[refIdx].pipeline_flags  |= VA_PROC_PIPELINE_FAST;
        m_pipelineParam[refIdx].filter_flags    |= VA_FILTER_SCALING_FAST;

        m_pipelineParam[refIdx].filters  = m_filterBufs;
        m_pipelineParam[refIdx].num_filters  = 0;

        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextVPP,
                            VAProcPipelineParameterBufferType,
                            sizeof(VAProcPipelineParameterBuffer),
                            1,
                            &m_pipelineParam[refIdx],
                            &m_pipelineParamID[refIdx]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
        vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[refIdx], 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        /*for frames 7, 14, 21, ...
         * or for the last frame*/
        if ( ((refIdx % 7) ==0) || ((refCount + 1) == refIdx) )
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaEndPicture");
            vaSts = vaEndPicture(m_vaDisplay, m_vaContextVPP);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }        
    } /* for( refIdx = 1; refIdx <= (pParams->fwdRefCount); refIdx++ )*/
    MFX_LTRACE_2(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "A|VPP|COMP|PACKET_END|", "%d|%d", m_vaContextVPP, 0);

    for( refIdx = 0; refIdx < m_pipelineParamCompID.size(); refIdx++ )
    {
        if ( m_pipelineParamCompID[refIdx] != VA_INVALID_ID)
        {
            vaSts = vaDestroyBuffer(m_vaDisplay, m_pipelineParamCompID[refIdx]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            m_pipelineParamCompID[refIdx] = VA_INVALID_ID;
        }
    }

    for( refIdx = 0; refIdx < m_pipelineParamID.size(); refIdx++ )
    {
        if ( m_pipelineParamID[refIdx] != VA_INVALID_ID)
        {
            vaSts = vaDestroyBuffer(m_vaDisplay, m_pipelineParamID[refIdx]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
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
} // mfxStatus VAAPIVideoProcessing::Execute_Composition(mfxExecuteParams *pParams)



mfxStatus VAAPIVideoProcessing::QueryTaskStatus(mfxU32 taskIndex)
{
    VAStatus vaSts;

#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
    VASurfaceID waitSurface = VA_INVALID_SURFACE;
    mfxU32 indxSurf = 0;

    // (1) find params (sutface & number) are required by feedbackNumber
    //-----------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        for (indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++)
        {
            if (m_feedbackCache[indxSurf].number == taskIndex)
            {
                waitSurface = m_feedbackCache[indxSurf].surface;
                break;
            }
        }
        if (VA_INVALID_SURFACE == waitSurface)
        {
            return MFX_ERR_UNKNOWN;
        }

        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
    }

#if !defined(ANDROID)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "VPP vaSyncSurface");
        vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
#endif

    return MFX_TASK_DONE;
#else

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
                    iter = m_feedbackCache.erase(iter);
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
#endif
} // mfxStatus VAAPIVideoProcessing::QueryTaskStatus(mfxU32 taskIndex)

#endif // #if defined (MFX_VA_LINUX)
#endif // #if defined (MFX_VPP_ENABLE)
/* EOF */
