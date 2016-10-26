//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined (MFX_VA_WIN)

// dxva.h warning
#pragma warning(disable: 4201)

#include <assert.h>
#include "mfx_platform_headers.h"
#include "libmfx_core_d3d9.h"
#include "mfx_utils.h"
#include "fast_compositing_ddi.h"
#include <set>


static const FASTCOMP_CAPS2 Caps2_Default = 
{
    0,              // 0 used to indicate defaults
    0x01,           // TargetInterlacingModes  = progressive output only
    FALSE,          // bTargetInSysMemory
    FALSE,          // bProcampControl
    FALSE,          // bDeinterlacingControl
    FALSE,          // bDenoiseControl
    FALSE,          // bDetailControl
    FALSE,          // bFmdControl
    FALSE,          // bVariances
    FALSE,          // bSceneDetection
    0
};

static const DXVA2_AYUVSample16 Background_Default =
{   // Black BG
    0x8000,         // Cr
    0x8000,         // Cb
    0x0000,         // Y
    0xffff          // Alpha
};

static const DXVA2_ProcAmpValues ProcAmpValues_Default =
{ 
    //Fraction, Value
    { 0x0000, 0x00 },       // Brightness
    { 0x0000, 0x01 },       // Contrast
    { 0x0000, 0x00 },       // Hue
    { 0x0000, 0x01 }        // Saturation
};

static const DXVA2_ExtendedFormat DXVA2_ExtendedFormat_Default = 
{
    DXVA2_SampleProgressiveFrame,           // SampleFormat
    DXVA2_VideoChromaSubsampling_MPEG2,     // VideoChromaSubsampling
    DXVA2_NominalRange_Normal,              // NominalRange
    DXVA2_VideoTransferMatrix_BT709,        // VideoTransferMatrix
    DXVA2_VideoLighting_dim,                // VideoLighting
    DXVA2_VideoPrimaries_BT709,             // VideoPrimaries
    DXVA2_VideoTransFunc_709                // VideoTransferFunction
};

static const FASTCOMP_BLT_PARAMS FastCompBlt_Default =
{
    NULL,                               // pRenderTarget
    0,                                  // SampleCount
    NULL,                               // pSamples
    0,                                  // TargetFrame
    { 0, 0, 0, 0 },                     // TargetRect
    FALSE,                              // ReservedCR
    DXVA2_VideoTransferMatrix_BT709,    // TargetMatrix
    FALSE,                              // TargetExtGamut
    FASTCOMP_TARGET_PROGRESSIVE,        // TargetIntMode
    FALSE,                              // TargetInSysMem
    0,                                  // Reserved1
    sizeof(FASTCOMP_BLT_PARAMS),        // iSizeOfStructure
    0,                                  // Reserved2
    NULL,                               // pReserved1
    Background_Default,                 // BackgroundColor
    NULL,                               // pReserved2
    // Rev 1.4 parameters
    ProcAmpValues_Default,              // ProcAmpValues
    FALSE,                              // bDenoiseAutoAdjust
    FALSE,                              // bFMDEnable
    FALSE,                              // bSceneDetectionEnable
    0,                                  // iDeinterlacingAlgorithm
    0,                                  // Reserved3
    0,                                  // wDenoiseFactor
    0,                                  // wDetailFactor
    0,                                  // wSpatialComplexity
    0,                                  // wTemporalComplexity
    0,                                  // iVarianceType
    NULL,                               // pVariances
    0,                                  // iCadence
    FALSE,                              // bRepeatFrame
    0,                                  // Reserved4
    0                                   // iFrameNum
};

const DXVA2_AYUVSample16 BLACK16 = {0x8000, 0x8000, 0x1000, 0x00FF};

using namespace MfxHwVideoProcessing;

RECT KeepAspectRatio(const RECT& dst, const RECT& src)
{
    RECT rect;

    mfxU32 src_w = src.right - src.left;
    mfxU32 src_h = src.bottom - src.top;
    mfxU32 dst_w = dst.right - dst.left;
    mfxU32 dst_h = dst.bottom - dst.top;

    mfxF32 rx = (mfxF32)dst_w / src_w;
    mfxF32 ry = (mfxF32)dst_h / src_h;
    mfxF32 r = (rx < ry) ? rx : ry;

    mfxU32 dx = mfxU32(dst_w - src_w * r)/2;
    mfxU32 dy = mfxU32(dst_h - src_h * r)/2;

    rect.left   = dst.left + dx;
    rect.right  = dst.right - dx;
    rect.top    = dst.top + dy;
    rect.bottom = dst.bottom - dy;

    return rect;

} // RECT KeepAspectRatio(const RECT& dst, const RECT& src)


DXVA2_SampleFormat MapPictureStructureToDXVAType(const mfxU32 PicStruct)
{
    if (PicStruct & MFX_PICSTRUCT_PROGRESSIVE)
    {
        return DXVA2_SampleProgressiveFrame;
    }
    else if (PicStruct & MFX_PICSTRUCT_FIELD_TFF)
    {
        return DXVA2_SampleFieldInterleavedEvenFirst;
    }
    else if (PicStruct & MFX_PICSTRUCT_FIELD_BFF)
    {
        return DXVA2_SampleFieldInterleavedOddFirst;
    }
    else
    {
        return DXVA2_SampleUnknown;
    }

} // DXVA2_SampleFormat MapPictureStructureToDXVAType(const mfxU32 PicStruct)



FastCompositingDDI::FastCompositingDDI(FASTCOMP_MODE iMode)
:m_bIsPresent(FALSE)
,m_bIsSystemSurface(FALSE)
,m_pSystemMemorySurface(NULL)
,m_iMode(iMode)
,m_formatSupport()
,m_inputSamples()
,m_cachedReadyTaskIndex()
,m_pDummySurface(NULL)
,m_pD3DDecoderService(NULL)
,m_pAuxDevice(NULL)
{
    memset(&m_caps,         0, sizeof(FASTCOMP_CAPS));
    memset(&m_caps2,        0, sizeof(FASTCOMP_CAPS2));
    memset(&m_frcCaps,      0, sizeof(FASTCOMP_FRC_CAPS));
    memset(&m_varianceCaps, 0, sizeof(FASTCOMP_VARIANCE_CAPS));
    memset(&m_frcState,     0, sizeof(D3D9Frc));
} // FastCompositingDDI::FastCompositingDDI(FASTCOMP_MODE iMode)

FastCompositingDDI::~FastCompositingDDI()
{
    Release();

} // FastCompositingDDI::~FastCompositingDDI()

mfxStatus FastCompositingDDI::CreateDevice(VideoCORE * core, mfxVideoParam* par, bool isTemporal)
{
    MFX_CHECK_NULL_PTR1( core );
    par;

    D3D9Interface *hwCore = QueryCoreInterface<D3D9Interface>(core);
    MFX_CHECK_NULL_PTR1( hwCore );

    IDirectXVideoDecoderService *pVideoService = NULL;

    mfxStatus sts = hwCore->GetD3DService(1920, 1080, &pVideoService, isTemporal);
    MFX_CHECK_STS(sts);

    sts = Initialize(hwCore->GetD3D9DeviceManager(), pVideoService);

    return sts;

} // mfxStatus FastCompositingDDI::CreateDevice(VideoCORE * core)


mfxStatus FastCompositingDDI::DestroyDevice(void)
{
    mfxStatus sts = Release();

    return sts;

} // mfxStatus FastCompositingDDI::DestroyDevice(void)


mfxStatus FastCompositingDDI::Initialize(
    IDirect3DDeviceManager9  *pD3DDeviceManager, 
    IDirectXVideoDecoderService *pVideoDecoderService)
{
    mfxStatus sts;
    HRESULT hRes;

    // release object before allocation
    Release();

    // create auxiliary device
    m_pAuxDevice = new AuxiliaryDevice();

    sts = m_pAuxDevice->Initialize(pD3DDeviceManager, pVideoDecoderService);
    MFX_CHECK(!sts, MFX_ERR_DEVICE_FAILED);

    // check video decoder service availability
    if (!pVideoDecoderService)
    {
        MFX_CHECK(pD3DDeviceManager, MFX_ERR_NOT_INITIALIZED);
        
        HANDLE hDXVideoDeviceHandle;

        hRes = pD3DDeviceManager->OpenDeviceHandle(&hDXVideoDeviceHandle);

        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        hRes = pD3DDeviceManager->GetVideoService(hDXVideoDeviceHandle, 
                                                IID_IDirectXVideoDecoderService, 
                                                (void**)&pVideoDecoderService);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        pD3DDeviceManager->CloseDeviceHandle(hDXVideoDeviceHandle);
    }

    MFX_CHECK(pVideoDecoderService, MFX_ERR_NOT_INITIALIZED);

    this->m_pD3DDecoderService = pVideoDecoderService;    


    sts = QueryCaps(m_caps);
    MFX_CHECK(!sts, MFX_ERR_DEVICE_FAILED);

    sts = QueryCaps2(m_caps2);
    MFX_CHECK(!sts, MFX_ERR_DEVICE_FAILED);

    sts = QueryFrcCaps(m_frcCaps);
    MFX_CHECK_STS(sts);

    // get variances
    memset((void*)&m_varianceCaps, 0, sizeof(FASTCOMP_VARIANCE_CAPS));
    if(m_caps2.bVariances)
    {
        sts = QueryVarianceCaps(&m_varianceCaps);
        MFX_CHECK_STS(sts);
    }
    // create acceleration device
    DXVA2_VideoDesc VideoDesc;
    VideoDesc.SampleWidth = m_caps.sPrimaryVideoCaps.iMaxWidth;
    VideoDesc.SampleHeight = m_caps.sPrimaryVideoCaps.iMaxHeight;
    VideoDesc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
    VideoDesc.SampleFormat.NominalRange = DXVA2_NominalRange_Normal;
    VideoDesc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
    VideoDesc.SampleFormat.VideoLighting = DXVA2_VideoLighting_dim;
    VideoDesc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
    VideoDesc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
    
    // TBD need params
   // VideoDesc.SampleFormat.SampleFormat = DXVA2_SampleFieldInterleavedEvenFirst;

    VideoDesc.Format = (D3DFORMAT)MAKEFOURCC('N','V','1','2');
    VideoDesc.InputSampleFreq.Numerator = 30000; 
    VideoDesc.InputSampleFreq.Denominator  = 1001;
    VideoDesc.OutputFrameFreq.Numerator = 60000;
    VideoDesc.OutputFrameFreq.Denominator = 1001;
    VideoDesc.UABProtectionLevel = 0;
    VideoDesc.Reserved = 0;

    FASTCOMP_CREATEDEVICE sCreateStruct = {&VideoDesc, (D3DFORMAT)MAKEFOURCC('N','V','1','2'), 0, m_iMode};
    UINT  uCreateSize = sizeof(sCreateStruct);

    hRes = m_pAuxDevice->CreateAccelerationService(DXVA2_FastCompositing, &sCreateStruct, uCreateSize);
    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

    //m_bIsRunning = TRUE;

    // FRC
    memset( &m_frcState, 0, sizeof(D3D9Frc));

    UINT uQuerySize;
    FASTCOMP_QUERYCAPS sQuery;
    memset(&sQuery, 0, sizeof(FASTCOMP_QUERYCAPS));
    sQuery.Type = FASTCOMP_QUERY_VPP_FORMAT_COUNT;

    uQuerySize = sizeof(FASTCOMP_QUERYCAPS);

    sts = m_pAuxDevice->QueryAccelCaps(&DXVA2_FastCompositing, &sQuery, &uQuerySize);

    sQuery.sFormats.pPrimaryFormats = new D3DFORMAT[sQuery.sFormats.iPrimaryFormatSize      +
                                                    sQuery.sFormats.iSecondaryFormatSize    +
                                                    sQuery.sFormats.iSubstreamFormatSize    +
                                                    sQuery.sFormats.iGraphicsFormatSize     +
                                                    sQuery.sFormats.iRenderTargetFormatSize +
                                                    sQuery.sFormats.iBackgroundFormatSize   ];

    sQuery.sFormats.pSecondaryFormats       = sQuery.sFormats.pPrimaryFormats      + sQuery.sFormats.iPrimaryFormatSize     ;
    sQuery.sFormats.pSubstreamFormats       = sQuery.sFormats.pSecondaryFormats    + sQuery.sFormats.iSecondaryFormatSize   ;
    sQuery.sFormats.pGraphicsFormats        = sQuery.sFormats.pSubstreamFormats    + sQuery.sFormats.iSubstreamFormatSize   ;
    sQuery.sFormats.pRenderTargetFormats    = sQuery.sFormats.pGraphicsFormats     + sQuery.sFormats.iGraphicsFormatSize    ;
    sQuery.sFormats.pBackgroundFormats      = sQuery.sFormats.pRenderTargetFormats + sQuery.sFormats.iRenderTargetFormatSize;

    sQuery.sFormats.iPrimaryFormatSize      *= sizeof(D3DFORMAT);
    sQuery.sFormats.iSecondaryFormatSize    *= sizeof(D3DFORMAT);
    sQuery.sFormats.iSubstreamFormatSize    *= sizeof(D3DFORMAT);
    sQuery.sFormats.iGraphicsFormatSize     *= sizeof(D3DFORMAT);
    sQuery.sFormats.iRenderTargetFormatSize *= sizeof(D3DFORMAT);
    sQuery.sFormats.iBackgroundFormatSize   *= sizeof(D3DFORMAT);

    sQuery.Type = FASTCOMP_QUERY_VPP_FORMATS;
    sts = m_pAuxDevice->QueryAccelCaps(&DXVA2_FastCompositing, &sQuery, &uQuerySize);

    for (mfxU32 i = 0; i < sQuery.sFormats.iPrimaryFormatSize / sizeof(D3DFORMAT); i++)
    {
        if (sQuery.sFormats.pPrimaryFormats[i] == D3DFMT_A8R8G8B8 || sQuery.sFormats.pPrimaryFormats[i] == D3DFMT_X8R8G8B8)
            m_formatSupport[MFX_FOURCC_RGB4] |= MFX_FORMAT_SUPPORT_INPUT;
        else if (sQuery.sFormats.pPrimaryFormats[i] == D3DFMT_A8B8G8R8 || sQuery.sFormats.pPrimaryFormats[i] == D3DFMT_X8B8G8R8)
            m_formatSupport[MFX_FOURCC_BGR4] |= MFX_FORMAT_SUPPORT_INPUT;
        else if (sQuery.sFormats.pPrimaryFormats[i] == D3DFMT_A2R10G10B10)
            m_formatSupport[MFX_FOURCC_A2RGB10] |= MFX_FORMAT_SUPPORT_INPUT;
        else
            m_formatSupport[sQuery.sFormats.pPrimaryFormats[i]] |= MFX_FORMAT_SUPPORT_INPUT;
    }

    for (mfxU32 i = 0; i < sQuery.sFormats.iRenderTargetFormatSize / sizeof(D3DFORMAT); i++)
    {
        if (sQuery.sFormats.pRenderTargetFormats[i] == D3DFMT_A8R8G8B8 || sQuery.sFormats.pRenderTargetFormats[i] == D3DFMT_X8R8G8B8)
            m_formatSupport[MFX_FOURCC_RGB4] |= MFX_FORMAT_SUPPORT_OUTPUT;
        else if (sQuery.sFormats.pRenderTargetFormats[i] == D3DFMT_A8B8G8R8 || sQuery.sFormats.pRenderTargetFormats[i] == D3DFMT_X8B8G8R8)
            m_formatSupport[MFX_FOURCC_BGR4] |= MFX_FORMAT_SUPPORT_OUTPUT;
        else if (sQuery.sFormats.pRenderTargetFormats[i] == D3DFMT_A2R10G10B10)
            m_formatSupport[MFX_FOURCC_A2RGB10] |= MFX_FORMAT_SUPPORT_OUTPUT;
        else
            m_formatSupport[sQuery.sFormats.pRenderTargetFormats[i]] |= MFX_FORMAT_SUPPORT_OUTPUT;
    }

    delete[] sQuery.sFormats.pPrimaryFormats;

    sQuery.sFormats.pPrimaryFormats      = 0;
    sQuery.sFormats.pSecondaryFormats    = 0;
    sQuery.sFormats.pSubstreamFormats    = 0;
    sQuery.sFormats.pGraphicsFormats     = 0;
    sQuery.sFormats.pRenderTargetFormats = 0;
    sQuery.sFormats.pBackgroundFormats   = 0;

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::Initialize(IDirect3DDeviceManager9  *pD3DDeviceManager, IDirectXVideoDecoderService *pVideoDecoderService)


mfxStatus FastCompositingDDI::Release()
{
    if (m_pAuxDevice)
    {
        m_pAuxDevice->Release();
    }

    delete m_pAuxDevice;
    m_pAuxDevice = NULL;

    if (m_pDummySurface)
    {
        m_pDummySurface->Release();
    }

    if (m_pSystemMemorySurface)
    {
        // unregister surface
        Register( 
            reinterpret_cast<mfxHDLPair*>(&m_pSystemMemorySurface), 
            1, 
            FALSE);

        m_pSystemMemorySurface->Release();
    }
    m_pDummySurface = NULL;

    memset( &m_frcState, 0, sizeof(D3D9Frc));

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::Release()


mfxStatus FastCompositingDDI::Register(mfxHDLPair* pSurfaces, mfxU32 num, BOOL bRegister)
{
    mfxStatus sts;

    
    sts = m_pAuxDevice->Register(
        reinterpret_cast<IDirect3DSurface9**>(&pSurfaces->first), 
        num, 
        bRegister);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::Register(IDirect3DSurface9 **pSurfaces, mfxU32 num, BOOL bRegister)


mfxStatus FastCompositingDDI::QueryCaps(FASTCOMP_CAPS& caps)
{
    mfxStatus sts;
    UINT uQuerySize;
    FASTCOMP_QUERYCAPS sQuery;

    if (FASTCOMP_MODE_PRE_PROCESS == m_iMode)
    {
        sQuery.Type = FASTCOMP_QUERY_VPP_CAPS;
    }
    else
    {
        sQuery.Type = FASTCOMP_QUERY_CAPS;
    }

    uQuerySize  = sizeof(sQuery);
    
    sts = m_pAuxDevice->QueryAccelCaps(&DXVA2_FastCompositing, &sQuery, &uQuerySize);
    MFX_CHECK(!sts, MFX_ERR_DEVICE_FAILED);

    memcpy_s(&caps, sizeof(caps), &sQuery.sCaps, sizeof(FASTCOMP_CAPS));

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::QueryCaps(FASTCOMP_CAPS& caps)


mfxStatus FastCompositingDDI::QueryCaps2(FASTCOMP_CAPS2& caps2)
{
    mfxStatus sts;

    UINT uQuerySize;
    FASTCOMP_QUERYCAPS sQuery;

    if (FASTCOMP_MODE_PRE_PROCESS == m_iMode)
    {
        sQuery.Type = FASTCOMP_QUERY_VPP_CAPS2;
    }
    else
    {
        sQuery.Type = FASTCOMP_QUERY_CAPS2;
    }

    // set query size, for compatibility
    sQuery.sCaps2.dwSize = uQuerySize = sizeof(sQuery);

    // query caps2
    sts = m_pAuxDevice->QueryAccelCaps(&DXVA2_FastCompositing, &sQuery, &uQuerySize);
    // MFX_CHECK(!sts, MFX_ERR_DEVICE_FAILED);

    if (MFX_ERR_NONE == sts)
    {
        // get query data
        memcpy_s(&caps2, sizeof(caps2), &sQuery.sCaps2, sizeof(FASTCOMP_CAPS2));
    }
    else
    {
        caps2.dwSize = 0;
    }

    // set defaults values
    if ( caps2.dwSize < sizeof(FASTCOMP_CAPS2) )
    {
        caps2 = Caps2_Default;
    }

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::QueryCaps2(FASTCOMP_CAPS2& caps2)


mfxStatus FastCompositingDDI::QueryFrcCaps(FASTCOMP_FRC_CAPS& frcCaps)
{
    mfxStatus sts;

    FASTCOMP_QUERYCAPS  sQuery;
    UINT                uQuerySize;

    uQuerySize = sizeof(sQuery);

    // [VPreP] FRC
    sQuery.Type = FASTCOMP_QUERY_FRC_CAPS;

    sts = m_pAuxDevice->QueryAccelCaps(&DXVA2_FastCompositing, &sQuery, &uQuerySize);
    if(MFX_ERR_NONE == sts)
    {
        frcCaps = sQuery.sFrcCaps;
    }
    else
    {
        FASTCOMP_FRC_CAPS caps = {0};
        frcCaps = caps;
    }

    return MFX_ERR_NONE;

} // mfxU32 FastCompositingDDI::QueryFrcCaps(FASTCOMP_FRC_CAPS& frcCaps)

mfxStatus FastCompositingDDI::QueryVarianceCaps(
    FASTCOMP_VARIANCE_CAPS * pVarianceCaps)
{
    FASTCOMP_QUERYCAPS  sQuery;
    UINT                uQuerySize;

    sQuery.Type = FASTCOMP_QUERY_VPP_VARIANCE_CAPS;
    uQuerySize  = sizeof(sQuery);

    mfxStatus sts = m_pAuxDevice->QueryAccelCaps(&DXVA2_FastCompositing, &sQuery, &uQuerySize);
    if( MFX_ERR_NONE == sts )
    {
        *pVarianceCaps = sQuery.sVarianceCaps;
    }
    else
    {
        //FASTCOMP_VARIANCE_CAPS caps;// = {0};
        //*pVarianceCaps = caps;
    }

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::QueryVarianceCaps(...)

mfxStatus FastCompositingDDI::QueryCapabilities(mfxVppCaps& caps)
{


    caps.uMaxWidth = (mfxU32)m_caps.sPrimaryVideoCaps.iMaxWidth;
    caps.uMaxHeight = (mfxU32)m_caps.sPrimaryVideoCaps.iMaxHeight;

    caps.uRotation  = 0; // FastComposing DDI doesn't support rotation.

    caps.uMirroring = 1; // Mirroring supported thru special CM copy kernel

    if(TRUE == m_caps.sPrimaryVideoCaps.bAdvancedDI)
    {
        caps.uAdvancedDI = 1;
    }

    if(TRUE == m_caps.sPrimaryVideoCaps.bSimpleDI)
    {
        caps.uSimpleDI = 1;
    }

    if(TRUE == m_caps.sPrimaryVideoCaps.bDenoiseFilter)
    {
        caps.uDenoiseFilter = 1;
    }

    if(TRUE == m_caps2.bDetailControl)
    {
        caps.uDetailFilter = 1;
    }

    if(TRUE == m_caps2.bVariances)
    {
        caps.uVariance = 1;
    }

    if(TRUE == m_caps.sPrimaryVideoCaps.bProcAmp)
    {
        caps.uProcampFilter = 1;
    }

    if( TRUE == m_caps2.bISControl )
    {
        caps.uIStabFilter = 1;
    }

    if( TRUE == m_caps2.bFieldWeavingControl )
    {
        caps.uFieldWeavingControl = 1;
    }

    if( TRUE == m_caps2.bScalingModeControl )
    {
        caps.uScaling = 1;
    }

    caps.iNumBackwardSamples = m_caps.sPrimaryVideoCaps.iNumBackwardSamples;
    caps.iNumForwardSamples  = m_caps.sPrimaryVideoCaps.iNumForwardSamples;

    // FRC support
    if(m_frcCaps.iRateConversionCount > 0)
    {
        caps.uFrameRateConversion = 1;
        caps.frcCaps.customRateData.resize(m_frcCaps.iRateConversionCount);
        for(mfxU32 indx = 0; indx < m_frcCaps.iRateConversionCount; indx++ )
        {
            caps.frcCaps.customRateData[indx].bkwdRefCount     = m_frcCaps.sCustomRateData[indx].usNumBackwardSamples;
            caps.frcCaps.customRateData[indx].fwdRefCount      = m_frcCaps.sCustomRateData[indx].usNumForwardSamples;
            caps.frcCaps.customRateData[indx].inputFramesOrFieldPerCycle = m_frcCaps.sCustomRateData[indx].InputFramesOrFields;
            caps.frcCaps.customRateData[indx].outputIndexCountPerCycle= m_frcCaps.sCustomRateData[indx].OutputFrames;
            caps.frcCaps.customRateData[indx].customRate.FrameRateExtN = m_frcCaps.sCustomRateData[indx].CustomRate.Numerator;
            caps.frcCaps.customRateData[indx].customRate.FrameRateExtD = m_frcCaps.sCustomRateData[indx].CustomRate.Denominator;
        }
    }

    // FourCC support
    for (mfxU32 indx = 0; indx < sizeof(g_TABLE_SUPPORTED_FOURCC)/sizeof(mfxU32); indx++)
    {
        caps.mFormatSupport[g_TABLE_SUPPORTED_FOURCC[indx]] = m_formatSupport[g_TABLE_SUPPORTED_FOURCC[indx]];
    }

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::QueryCapabilities(mfxVppCaps& caps)


enum QueryStatus
{
    VPREP_GPU_READY         =   0,
    VPREP_GPU_BUSY          =   1,
    VPREP_GPU_NOT_REACHED   =   2,
    VPREP_GPU_FAILED        =   3
};

mfxStatus FastCompositingDDI::QueryTaskStatus(mfxU32 taskIndex)
{
    HRESULT hRes;

    const mfxU32 numStructures = 6 * 2;
    FASTCOMP_QUERY_STATUS queryStatus[numStructures];

    std::set<mfxU32>::iterator iterator;
    for (mfxU32 i = 0; i < numStructures; i += 1)
    {
        queryStatus[i].Status = VPREP_GPU_FAILED;
    }
    
    // execute call
    hRes = m_pAuxDevice->Execute(
        FASTCOMP_QUERY_STATUS_FUNC_ID, 
        NULL, 
        0, 
        &queryStatus, 
        numStructures*sizeof(FASTCOMP_QUERY_STATUS));

    // houston, we have a problem :)
    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

    for (mfxU32 i = 0; i < numStructures; i += 1)
    {
        if (VPREP_GPU_READY == queryStatus[i].Status)
        {
            m_cachedReadyTaskIndex.insert(queryStatus[i].QueryStatusID);
        }
        else if (VPREP_GPU_FAILED == queryStatus[i].Status)
        {
            return MFX_ERR_DEVICE_FAILED;
        }
    }

    iterator = find(m_cachedReadyTaskIndex.begin(), m_cachedReadyTaskIndex.end(), taskIndex);
    
    if (m_cachedReadyTaskIndex.end() == iterator)
    {
        return MFX_TASK_BUSY;
    }

    m_cachedReadyTaskIndex.erase(iterator);

    return MFX_TASK_DONE;

} // mfxStatus FastCompositingDDI::QueryTaskStatus(mfxU32 taskIndex)


void FastCompositingDDI::ResetBltParams(FASTCOMP_BLT_PARAMS *pBlt)
{
    if (NULL != pBlt)
    {
        *pBlt = FastCompBlt_Default;
    }

} // void FastCompositingDDI::ResetBltParams(FASTCOMP_BLT_PARAMS *pBlt)

mfxStatus FastCompositingDDI::FastCopy(IDirect3DSurface9 *pTarget, IDirect3DSurface9 *pSource,
                                       RECT *pTargetRect, RECT *pSourceRect, BOOL bInterlaced)
{
    D3DSURFACE_DESC surfDesc;

    FASTCOMP_BLT_PARAMS videoCompositingBlt = FastCompBlt_Default;

    IDirect3DSurface9 *pSourceArray[]  = { pTarget, pSource };
    FASTCOMP_VideoSample samples[1];

    // check appropriate capabilities
    if (!m_caps2.bTargetInSysMemory)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    // obtain surface description
    pTarget->GetDesc(&surfDesc);

    if (surfDesc.Pool != D3DPOOL_SYSTEMMEM)
    {
        return MFX_ERR_UNKNOWN;
    }

    // set output mode
    if (m_caps2.TargetInterlacingModes & FASTCOMP_TARGET_NO_DEINTERLACING_MASK)
    {
        videoCompositingBlt.iTargetInterlacingMode = FASTCOMP_TARGET_NO_DEINTERLACING;
    }
    else
    {
        videoCompositingBlt.iTargetInterlacingMode = FASTCOMP_TARGET_PROGRESSIVE;
    }

    // check support for interlaced to interlaced copy
    if (bInterlaced)
    {
        if (FASTCOMP_TARGET_PROGRESSIVE == videoCompositingBlt.iTargetInterlacingMode)
        {
            // fast copy doesn't support of interlaced output
            return MFX_ERR_UNSUPPORTED;
        }

        // check for interlaced scaling
        if (memcmp(pTargetRect, pSourceRect, sizeof(RECT)) &&
            0 == m_caps.sPrimaryVideoCaps.bInterlacedScaling)
        {
            // fast copy doesn't support of interlaced scaling
            return MFX_ERR_UNSUPPORTED;
        }
    }

    // register source and target surfaces
    m_pAuxDevice->Register(pSourceArray, 2, TRUE);

    // set fast compositing parameters for fast copy
    videoCompositingBlt.pRenderTarget = pTarget;
    videoCompositingBlt.SampleCount = 1;
    videoCompositingBlt.pSamples = samples;
    videoCompositingBlt.TargetFrame = 0;
    videoCompositingBlt.TargetRect = *pTargetRect;
    videoCompositingBlt.bTargetInSysMemory = TRUE;

    // set main video
    ZeroMemory(samples, sizeof(samples));
    samples[0].Depth = FASTCOMP_DEPTH_MAIN_VIDEO;
    samples[0].SrcSurface = pSource;
    samples[0].SrcRect = *pSourceRect;
    samples[0].DstRect = *pTargetRect;
    samples[0].Start = 0;
    samples[0].End = 1000;

    // wet interlaced/progressive sample flag
    samples[0].SampleFormat = DXVA2_ExtendedFormat_Default;
    samples[0].SampleFormat.SampleFormat = (bInterlaced) ? 
        DXVA_SampleFieldInterleavedEvenFirst : DXVA_SampleProgressiveFrame;

    // fast compositing blt
    // TODO !!!
    m_pAuxDevice->Execute(FASTCOMP_BLT, (void *) &videoCompositingBlt, 
                          sizeof(FASTCOMP_BLT_PARAMS), NULL, 0);

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::FastCopy(IDirect3DSurface9 *pTarget, IDirect3DSurface9 *pSource, RECT *pTargetRect, RECT *pSourceRect, BOOL bInterlaced)


mfxStatus FastCompositingDDI::QueryVariance(
    mfxU32 frameIndex, 
    std::vector<UINT> &variance)
{
    HRESULT hRes;
    
    variance.resize(m_varianceCaps.VarianceCount);
    if(!m_caps2.bVariances || variance.empty() ) return MFX_ERR_NONE;

    FASTCOMP_QUERY_VARIANCE_PARAMS queryVarianceParam = {0};

    queryVarianceParam.FrameNumber = frameIndex;
    queryVarianceParam.pVariances  = &variance[0];
    queryVarianceParam.VarianceBufferSize = m_varianceCaps.VarianceCount * m_varianceCaps.VarianceSize; //variance.size();
 
    // execute call
    hRes = m_pAuxDevice->Execute(
        FASTCOMP_QUERY_VARIANCE, 
        (void *) &queryVarianceParam, 
        sizeof(FASTCOMP_QUERY_VARIANCE_PARAMS), 
        NULL, 
        0);
  
    // houston, we have a problem :)
    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

    /*if (querySceneDetection.QueryMode)
    {
        sceneChangeRate = (querySceneDetection.CurFieldSceneChangeRate + querySceneDetection.PreFieldSceneChangeRate) / 2;
    }
    else
    {
        sceneChangeRate = querySceneDetection.FrameSceneChangeRate;
    }*/
  
    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::QueryVariance(...)


mfxStatus FastCompositingDDI::ExecuteBlt(FASTCOMP_BLT_PARAMS *pBltParams)
{
    HRESULT hRes;

    // fast compositing blt
    hRes = m_pAuxDevice->Execute(FASTCOMP_BLT, (void *)pBltParams, 
                                 sizeof(FASTCOMP_BLT_PARAMS), NULL, 0);

    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::ExecuteBlt(FASTCOMP_BLT_PARAMS *pBltParams)


mfxStatus FastCompositingDDI::ConvertExecute2BltParams( mfxExecuteParams *pExecuteParams, FASTCOMP_BLT_PARAMS *pBltParams )
{
    // procamp
    pBltParams->ProcAmpValues.Brightness = DXVA2FloatToFixed((mfxF32)pExecuteParams->Brightness);
    pBltParams->ProcAmpValues.Saturation = DXVA2FloatToFixed((mfxF32)pExecuteParams->Saturation);
    pBltParams->ProcAmpValues.Hue        = DXVA2FloatToFixed((mfxF32)pExecuteParams->Hue);
    pBltParams->ProcAmpValues.Contrast   = DXVA2FloatToFixed((mfxF32)pExecuteParams->Contrast);

    // pipeline
    pBltParams->iDeinterlacingAlgorithm  = pExecuteParams->iDeinterlacingAlgorithm;

    pBltParams->bDenoiseAutoAdjust       = pExecuteParams->bDenoiseAutoAdjust;
    pBltParams->wDenoiseFactor           = pExecuteParams->denoiseFactor;

    pBltParams->wDetailFactor            = pExecuteParams->detailFactor;
    pBltParams->bFMDEnable               = pExecuteParams->bFMDEnable;
//    pBltParams->bSceneDetectionEnable    = pExecuteParams->bSceneDetectionEnable;

    pBltParams->iTargetInterlacingMode   = pExecuteParams->iTargetInterlacingMode;

    pBltParams->TargetFrame              = pExecuteParams->targetTimeStamp;   

    pBltParams->bFieldWeaving            = pExecuteParams->bFieldWeaving;

    //Variance
    pBltParams->bVarianceQuery           = pExecuteParams->bVarianceEnable;

    m_inputSamples.resize( pExecuteParams->refCount );

    // reference samples 
    for( int refIdx = 0; refIdx < pExecuteParams->refCount; refIdx++ )
    {
        mfxDrvSurface* pRefSurf = &(pExecuteParams->pRefSurfaces[refIdx]);
        FASTCOMP_VideoSample* pInputSample = &m_inputSamples[refIdx];

        memset(pInputSample, 0, sizeof(FASTCOMP_VideoSample));

        pInputSample->SrcSurface = (IDirect3DSurface9*)(size_t)(pRefSurf->hdl.first);
        pInputSample->Start      = pRefSurf->startTimeStamp;
        pInputSample->End        = pRefSurf->endTimeStamp;

        pInputSample->SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
        if((pRefSurf->frameInfo.FourCC == MFX_FOURCC_IMC3 ||
            pRefSurf->frameInfo.FourCC == MFX_FOURCC_YUV400 ||
            pRefSurf->frameInfo.FourCC == MFX_FOURCC_YUV411 ||
            pRefSurf->frameInfo.FourCC == MFX_FOURCC_YUV422H ||
            pRefSurf->frameInfo.FourCC == MFX_FOURCC_YUV422V ||
            pRefSurf->frameInfo.FourCC == MFX_FOURCC_YUV444) &&
            pExecuteParams->targetSurface.frameInfo.FourCC == MFX_FOURCC_RGB4 ||
            pRefSurf->frameInfo.FourCC == MFX_FOURCC_RGBP &&
           (pExecuteParams->targetSurface.frameInfo.FourCC == MFX_FOURCC_NV12 || 
            pExecuteParams->targetSurface.frameInfo.FourCC == MFX_FOURCC_YUY2))
        {
            pInputSample->SampleFormat.NominalRange = DXVA2_NominalRange_0_255;
            pInputSample->SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
            pBltParams->bTargetYuvFullRange = 1;
            pBltParams->TargetTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
        }
        else
        {
            if(pRefSurf->frameInfo.FourCC == MFX_FOURCC_RGB4)
            {
                pInputSample->SampleFormat.NominalRange = DXVA2_NominalRange_0_255;
            }
            pInputSample->SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
            //pBltParams->bTargetYuvFullRange = 1;
            pBltParams->TargetTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
        }

        /* Video signal info */
        size_t index = static_cast<size_t>(refIdx) < pExecuteParams->VideoSignalInfo.size() ? refIdx : ( pExecuteParams->VideoSignalInfo.size() - 1 );
        if (pExecuteParams->VideoSignalInfo[index].enabled)
        {
            if (pExecuteParams->VideoSignalInfo[index].NominalRange != MFX_NOMINALRANGE_UNKNOWN)
            {
                pInputSample->SampleFormat.NominalRange = (pExecuteParams->VideoSignalInfo[index].NominalRange == MFX_NOMINALRANGE_16_235) ? DXVA2_NominalRange_16_235 : DXVA2_NominalRange_0_255 ;
            }

            if (pExecuteParams->VideoSignalInfo[index].TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
            {
                pInputSample->SampleFormat.VideoTransferMatrix = (pExecuteParams->VideoSignalInfo[index].TransferMatrix == MFX_TRANSFERMATRIX_BT709) ? DXVA2_VideoTransferMatrix_BT709 : DXVA2_VideoTransferMatrix_BT601 ;
            }
        }

        if(pExecuteParams->VideoSignalInfoOut.enabled)
        {
            if (pExecuteParams->VideoSignalInfoOut.NominalRange != MFX_NOMINALRANGE_UNKNOWN)
            {
                pBltParams->bTargetYuvFullRange = (pExecuteParams->VideoSignalInfoOut.NominalRange == MFX_NOMINALRANGE_16_235) ? 0 : 1 ;
            }

            if (pExecuteParams->VideoSignalInfoOut.TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
            {
                pBltParams->TargetTransferMatrix = (pExecuteParams->VideoSignalInfoOut.TransferMatrix == MFX_TRANSFERMATRIX_BT709) ? DXVA2_VideoTransferMatrix_BT709 : DXVA2_VideoTransferMatrix_BT601 ;
            }

        }

        pInputSample->SampleFormat.VideoLighting = DXVA2_VideoLighting_Unknown;
        pInputSample->SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_Unknown;
        pInputSample->SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_Unknown;

        if ((pExecuteParams->bComposite) && (0 != refIdx))
        {
            // for sub-streams
            pInputSample->SampleFormat.SampleFormat = DXVA2_SampleSubStream;
            pInputSample->Depth = 2;
        }
        else
        {
            // primary stream, depth is 0
            pInputSample->Depth = 0;
            if(!pExecuteParams->bFieldWeaving)
            {
                pInputSample->SampleFormat.SampleFormat = MapPictureStructureToDXVAType(pRefSurf->frameInfo.PicStruct);
            }
            else
            {
                if(refIdx == 1) // for top field
                {
                    pInputSample->SampleFormat.SampleFormat = DXVA2_SampleFieldSingleEven;
                }
                else // for bottom field
                {
                    pInputSample->SampleFormat.SampleFormat = DXVA_SampleFieldSingleOdd;
                }
            }
        }

        // cropping
        // source cropping
        mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
        pInputSample->SrcRect.top    = inInfo->CropY;
        pInputSample->SrcRect.left   = inInfo->CropX;
        pInputSample->SrcRect.bottom = inInfo->CropH;
        pInputSample->SrcRect.right  = inInfo->CropW;
        pInputSample->SrcRect.bottom += inInfo->CropY;
        pInputSample->SrcRect.right  += inInfo->CropX;

        // destination cropping
        if (pExecuteParams->bComposite)
        {
            // for sub-streams use DstRect info from ext buffer set by app
            MFX_CHECK(refIdx < (int) pExecuteParams->dstRects.size(), MFX_ERR_UNKNOWN);
            const DstRect& rec = pExecuteParams->dstRects[refIdx];
            pInputSample->DstRect.top = rec.DstY;
            pInputSample->DstRect.left = rec.DstX;
            pInputSample->DstRect.bottom = rec.DstY + rec.DstH;
            pInputSample->DstRect.right  = rec.DstX + rec.DstW;

            pInputSample->Alpha     = (rec.GlobalAlphaEnable) ? rec.GlobalAlpha / 255.0f : 1.0f;
            pInputSample->bLumaKey  = rec.LumaKeyEnable;
            pInputSample->iLumaLow  = rec.LumaKeyMin;
            pInputSample->iLumaHigh = rec.LumaKeyMax;
        }
        else
        {
            mfxFrameInfo *outInfo = &(pExecuteParams->targetSurface.frameInfo);
            pInputSample->DstRect.top = outInfo->CropY;
            pInputSample->DstRect.left = outInfo->CropX;
            pInputSample->DstRect.bottom = outInfo->CropH;
            pInputSample->DstRect.right  = outInfo->CropW;
            pInputSample->DstRect.bottom += outInfo->CropY;
            pInputSample->DstRect.right += outInfo->CropX;
        }
    }

    pBltParams->SampleCount = pExecuteParams->refCount;
    pBltParams->pSamples    = &m_inputSamples[0];

    pBltParams->TargetFrame   = pExecuteParams->targetTimeStamp;
    pBltParams->pRenderTarget = (IDirect3DSurface9*)(size_t)(pExecuteParams->targetSurface.hdl.first);
    
    // aya: FIXME rewritting
    // initialize region of interest
    RECT srcRect, outRect;
    srcRect.left = srcRect.top = outRect.left = outRect.top = 0;

    srcRect.right  = pExecuteParams->pRefSurfaces[0].frameInfo.Width;
    srcRect.bottom = pExecuteParams->pRefSurfaces[0].frameInfo.Height;

    outRect.right  = pExecuteParams->targetSurface.frameInfo.Width;
    outRect.bottom = pExecuteParams->targetSurface.frameInfo.Height;

    pBltParams->TargetRect = outRect;

    KeepAspectRatio(pBltParams->TargetRect, srcRect);

    pBltParams->BackgroundColor = BLACK16;

    mfxFrameInfo *outInfo = &(pExecuteParams->targetSurface.frameInfo);

    if (outInfo->FourCC == MFX_FOURCC_NV12 ||
        outInfo->FourCC == MFX_FOURCC_YV12 ||
        outInfo->FourCC == MFX_FOURCC_NV16 ||
        outInfo->FourCC == MFX_FOURCC_YUY2 ||
        outInfo->FourCC == MFX_FOURCC_P010 ||
        outInfo->FourCC == MFX_FOURCC_P210 ||
        outInfo->FourCC == MFX_FOURCC_AYUV )
    {
        pBltParams->BackgroundColor.Alpha = ((pExecuteParams->iBackgroundColor >> 48) & 0xff) << 8;
        pBltParams->BackgroundColor.Y     = ((pExecuteParams->iBackgroundColor >> 32) & 0xff) << 8;
        pBltParams->BackgroundColor.Cb    = ((pExecuteParams->iBackgroundColor >> 16) & 0xff) << 8;
        pBltParams->BackgroundColor.Cr    = ((pExecuteParams->iBackgroundColor      ) & 0xff) << 8;
    }
    if (outInfo->FourCC == MFX_FOURCC_Y210 ||
        outInfo->FourCC == MFX_FOURCC_Y410 )
    {
        pBltParams->BackgroundColor.Alpha = (pExecuteParams->iBackgroundColor >> 48) & 0xffff;
        pBltParams->BackgroundColor.Y     = (pExecuteParams->iBackgroundColor >> 32) & 0xffff;
        pBltParams->BackgroundColor.Cb    = (pExecuteParams->iBackgroundColor >> 16) & 0xffff;
        pBltParams->BackgroundColor.Cr    = (pExecuteParams->iBackgroundColor      ) & 0xffff;
    }
    if (outInfo->FourCC == MFX_FOURCC_RGB3    ||
        outInfo->FourCC == MFX_FOURCC_RGB4    ||
        outInfo->FourCC == MFX_FOURCC_BGR4    ||
        outInfo->FourCC == MFX_FOURCC_A2RGB10 ||
        outInfo->FourCC == MFX_FOURCC_ARGB16  ||
        outInfo->FourCC == MFX_FOURCC_R16)
    {
        pBltParams->BackgroundColor.Alpha = ((pExecuteParams->iBackgroundColor >> 48) & 0xff) << 8;

        USHORT R, G, B;

        R = (pExecuteParams->iBackgroundColor >> 32) & 0xff;
        G = (pExecuteParams->iBackgroundColor >> 16) & 0xff;
        B = (pExecuteParams->iBackgroundColor      ) & 0xff;
        
        pBltParams->BackgroundColor.Y  = (USHORT)(( 0x000041cb * R + 0x00008106 * G + 0x00001917 * B + 0x108000) >> 8);
        pBltParams->BackgroundColor.Cb = (USHORT)((-0x000025e3 * R - 0x00004a7f * G + 0x00007062 * B + 0x808000) >> 8);
        pBltParams->BackgroundColor.Cr = (USHORT)(( 0x00007062 * R - 0x00005e35 * G - 0x0000122d * B + 0x808000) >> 8);

        pBltParams->BackgroundColor.Y  = IPP_MAX( IPP_MIN(255 << 8, pBltParams->BackgroundColor.Y),  0); // CLIP(val, 0, 255 << 8)
        pBltParams->BackgroundColor.Cb = IPP_MAX( IPP_MIN(255 << 8, pBltParams->BackgroundColor.Cb), 0); // CLIP(val, 0, 255 << 8)
        pBltParams->BackgroundColor.Cr = IPP_MAX( IPP_MIN(255 << 8, pBltParams->BackgroundColor.Cr), 0); // CLIP(val, 0, 255 << 8)
    }

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::ConvertExecute2BltParams( mfxExecuteParams *pExecuteParams, FASTCOMP_BLT_PARAMS *pBltParams )


mfxStatus FastCompositingDDI::Execute(mfxExecuteParams *pParams)
{
    FASTCOMP_BLT_PARAMS bltParams = {0};

    mfxStatus sts = ConvertExecute2BltParams( pParams, &bltParams );
    MFX_CHECK_STS(sts);

    // set size of blt params structure
    bltParams.iSizeOfStructure = sizeof(FASTCOMP_BLT_PARAMS);

    // All process with Node structure should be done here
    //{
        FASTCOMP_QUERY_STATUS_PARAMS_V1_0 statusParams;
        statusParams.iQueryStatusID = pParams->statusReportID;

        FASTCOMP_BLT_PARAMS_OBJECT queryStatusObject;
        queryStatusObject.type = FASTCOMP_QUERY_STATUS_V1_0;
        queryStatusObject.pParams = (void *)&statusParams;
        queryStatusObject.iSizeofParams = sizeof(statusParams);

        bltParams.QueryStatusObject = queryStatusObject;
    //}

    //{
        FASTCOMP_IS_PARAMS_V1_0 istabParams;
        istabParams.IStabMode = pParams->bImgStabilizationEnable ? (FASTCOMP_ISTAB_MODE)pParams->istabMode : ISTAB_MODE_NONE;

        FASTCOMP_BLT_PARAMS_OBJECT istabObject;
        istabObject.type = FASTCOMP_FEATURES_IS_V1_0;
        istabObject.pParams = (void *)&istabParams;
        istabObject.iSizeofParams = sizeof(istabParams);

        if( pParams->bImgStabilizationEnable ) 
        {
            bltParams.ISObject = istabObject;
        }
        else
        {
            bltParams.ISObject.iSizeofParams = 0;
            bltParams.ISObject.pParams       = NULL;
            bltParams.ISObject.type          = FASTCOMP_FEATURES_IS_V1_0;
        }
    //}

    //{
        FASTCOMP_FRC_PARAMS_V1_0 frcParams = {0};
        FASTCOMP_BLT_PARAMS_OBJECT frcObject;
        if(pParams->bFRCEnable)
        {
            if(false == m_frcState.m_isInited)
            {
                m_frcState.m_inputFramesOrFieldsPerCycle = pParams->customRateData.inputFramesOrFieldPerCycle;
                m_frcState.m_outputIndexCountPerCycle    = pParams->customRateData.outputIndexCountPerCycle;
                m_frcState.m_isInited = true;
            }

            frcParams.InputFrameOrField = m_frcState.m_inputIndx;
            frcParams.OutputIndex       = m_frcState.m_outputIndx;
            frcParams.CustomRate.Numerator = pParams->customRateData.customRate.FrameRateExtN;
            frcParams.CustomRate.Denominator = pParams->customRateData.customRate.FrameRateExtD;
            frcParams.RepeatFrame = 0;//1 - repeat, 0 - interpolation 
            // update
            m_frcState.m_outputIndx++;
            if(m_frcState.m_outputIndexCountPerCycle == m_frcState.m_outputIndx)
            {
                m_frcState.m_outputIndx = 0;
                m_frcState.m_inputIndx += m_frcState.m_inputFramesOrFieldsPerCycle;
            }

            frcObject.type = FASTCOMP_FEATURES_FRC_V1_0;
            frcObject.pParams = (void *)&frcParams;
            frcObject.iSizeofParams = sizeof(frcParams);
        }
        else
        {
            frcObject.type          = FASTCOMP_FEATURES_FRC_V1_0;
            frcObject.pParams       = NULL;
            frcObject.iSizeofParams = 0;
        }

        bltParams.FRCObject = frcObject;
    //}

    //{
        FASTCOMP_SCALINGMODE_PARAMS_V1_0 scalingParams = {0};
        FASTCOMP_BLT_PARAMS_OBJECT scalingObject;

        if (m_caps2.bScalingModeControl)
        {
            switch(pParams->scalingMode)
            {
            case MFX_SCALING_MODE_LOWPOWER:
                scalingParams.FastMode = false;
                break;

            case MFX_SCALING_MODE_QUALITY:
            case MFX_SCALING_MODE_DEFAULT:
            default:
                scalingParams.FastMode = true;
                break;
            }

            scalingObject.type          = FASTCOMP_FEATURES_SCALINGMODE_V1_0;
            scalingObject.pParams       = (void *)&scalingParams;
            scalingObject.iSizeofParams = sizeof(scalingParams);
        }
        else
        {
            scalingObject.type          = FASTCOMP_FEATURES_SCALINGMODE_V1_0;
            scalingObject.pParams       = NULL;
            scalingObject.iSizeofParams = 0;
        }

        bltParams.ScalingModeObject = scalingObject;
    //}

    sts = ExecuteBlt( &bltParams );
    MFX_CHECK_STS(sts);

    /*if( pParams->bVarianceEnable )
    {
        pParams->frameNumber = bltParams.iFrameNum;
    }*/

    return sts;

} // mfxStatus FastCompositingDDI::Execute(mfxExecuteParams *pParams)


mfxStatus FastCompositingDDI::CreateSurface(IDirect3DSurface9 **surface, 
                                            mfxU32 width, mfxU32 height, 
                                            D3DFORMAT format, 
                                            D3DPOOL pool)
{
    HRESULT hRes;

    if (m_pD3DDecoderService)
    {
         hRes = m_pD3DDecoderService->CreateSurface(width,
                                                    height,
                                                    0,
                                                    format,
                                                    pool,
                                                    0,
                                                    DXVA2_VideoSoftwareRenderTarget,
                                                    surface,
                                                    NULL);

        if (FAILED(hRes))
        {
            return MFX_ERR_DEVICE_FAILED;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus FastCompositingDDI::CreateSurface(IDirect3DSurface9 **surface, mfxU32 width, mfxU32 height, D3DFORMAT format, D3DPOOL pool)

#endif // #if defined (MFX_VA)
#endif // #if defined (MFX_ENABLE_VPP)
