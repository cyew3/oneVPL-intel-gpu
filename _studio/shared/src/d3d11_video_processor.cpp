/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION

This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement

Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

File Name: d3d11_video_processor.cpp

\* ****************************************************************************** */
#if defined (MFX_D3D11_ENABLED)

#include <math.h>

#include "vm_file.h"
#include "d3d11_video_processor.h"

#include "umc_va_dxva2_protected.h"
#include "libmfx_core_d3d11.h"
#include "mfx_platform_headers.h"
#include "libmfx_core_interface.h"

//using namespace MfxHwVideoProcessing;

//#define DEBUG_DETAIL_INFO
//#define DUMP_REFERENCE



#define CHECK_HRES(hRes) \
    if (FAILED(hRes))\
    return MFX_ERR_DEVICE_FAILED;

#define D3DFMT_NV12 (DXGI_FORMAT)MAKEFOURCC('N','V','1','2')
#define D3DFMT_YV12 (DXGI_FORMAT)MAKEFOURCC('Y','V','1','2')

#define SAFE_DELETE_ARRAY(ptr) if (ptr) { delete [] ptr; ptr = 0; }

template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }
template<class T> inline void Zero(std::vector<T> & vec)   { memset(&vec[0], 0, sizeof(T) * vec.size()); }
template<class T> inline void Zero(T * first, size_t cnt)  { memset(first, 0, sizeof(T) * cnt); }

//---------------------------------------------------------
namespace MfxVppDump
{
    struct FrameLocker
    {
        FrameLocker(VideoCORE * core, mfxFrameData & data, bool external = false)
            : m_core(core)
            , m_data(data)
            , m_memId(data.MemId)
            , m_status(Lock(external))
        {
        }

        FrameLocker(VideoCORE * core, mfxFrameData & data, mfxMemId memId, bool external = false)
            : m_core(core)
            , m_data(data)
            , m_memId(memId)
            , m_status(Lock(external))
        {
        }

        ~FrameLocker() { Unlock(); }

        mfxStatus Unlock()
        {
            mfxStatus mfxSts = MFX_ERR_NONE;

            if (m_status == LOCK_INT)
                mfxSts = m_core->UnlockFrame(m_memId, &m_data);
            else if (m_status == LOCK_EXT)
                mfxSts = m_core->UnlockExternalFrame(m_memId, &m_data);

            m_status = LOCK_NO;
            return mfxSts;
        }

    protected:
        enum { LOCK_NO, LOCK_INT, LOCK_EXT };

        mfxU32 Lock(bool external)
        {
            mfxU32 status = LOCK_NO;

            if (m_data.Y == 0)
            {
                status = external
                    ? (m_core->LockExternalFrame(m_memId, &m_data) == MFX_ERR_NONE ? LOCK_EXT : LOCK_NO)
                    : (m_core->LockFrame(m_memId, &m_data) == MFX_ERR_NONE ? LOCK_INT : LOCK_NO);
            }

            return status;
        }

    private:
        FrameLocker(FrameLocker const &);
        FrameLocker & operator =(FrameLocker const &);

        VideoCORE *    m_core;
        mfxFrameData & m_data;
        mfxMemId       m_memId;
        mfxU32         m_status;
    };

    void WriteFrameData(
        vm_file *            file,
        VideoCORE *          core,
        mfxFrameData const & fdata,
        mfxFrameInfo const & info,
        bool external);
};
//---------------------------------------------------------


template<typename T> mfxU32 FindFree(std::vector<T> const & vec)
{
    for (size_t j = 0; j < vec.size(); j++)
    {
        if (vec[j].IsFree())
        {
            return (mfxU32)j;
        }
    }

    return NO_INDEX;

} // template<typename T> mfxU32 FindFree(std::vector<T> const & vec)


D3D11_VIDEO_FRAME_FORMAT D3D11PictureStructureMapping(const mfxU32 PicStruct)
{
    if (PicStruct & MFX_PICSTRUCT_PROGRESSIVE)
    {
        return D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    }
    else if (PicStruct & MFX_PICSTRUCT_FIELD_TFF)
    {
        return D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
    }
    else if (PicStruct & MFX_PICSTRUCT_FIELD_BFF)
    {
        return D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
    }
    else
    {
        return D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    }

} // DXVA2_SampleFormat MapPictureStructureToDXVAType(const mfxU32 PicStruct)

DXGI_RATIONAL D3D11FrameRateMapping(const mfxU32 frmD, const mfxU32 frmN)
{
    DXGI_RATIONAL frm = {frmN, frmD};
    return frm;

} // DXGI_RATIONAL D3D11FrameRateMapping(const mfxU32 frmD, const mfxU32 frmN)


void printGuid(GUID guid)
{
    //CoCreateGuid(&guid);

    OLECHAR* bstrGuid;
    StringFromCLSID(guid, &bstrGuid);
    //wprintf(L"%s\n", ws);
    wprintf(L"%s", bstrGuid);

    ::CoTaskMemFree(bstrGuid);
}

using namespace MfxHwVideoProcessing;


mfxStatus D3D11VideoProcessor::CreateDevice(VideoCORE * pCore,  mfxVideoParam* par, bool isTemporal)
{
    MFX_CHECK_NULL_PTR1(pCore);

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(pCore);

    MFX_CHECK_NULL_PTR1(pD3d11);

    mfxStatus sts = Init(
        pD3d11->GetD3D11Device(isTemporal),
        pD3d11->GetD3D11VideoDevice(isTemporal),
        pD3d11->GetD3D11DeviceContext(isTemporal),
        pD3d11->GetD3D11VideoContext(isTemporal),
        par);

    m_core = pCore;

#ifdef DUMP_REFERENCE
    m_file =  vm_file_fopen(VM_STRING("reference.yuv"), VM_STRING("wb"));
#endif

    return sts;

} // mfxStatus mfxStatus D3D11VideoProcessor::CreateDevice(VideoCORE * pCore,  mfxVideoParam* par, bool isTemporal)


mfxStatus D3D11VideoProcessor::DestroyDevice(void)
{
    mfxStatus sts = Close();

    return sts;

} // mfxStatus D3D11VideoProcessor::DestroyDevice(void)


D3D11VideoProcessor::D3D11VideoProcessor(void)
{
    m_pVideoProcessor = NULL;
    m_pVideoProcessorEnum = NULL;
    m_CameraSet = false;
    m_pDevice = NULL;
    m_pVideoDevice = NULL;
    m_pDeviceContext = NULL;
    m_pVideoContext  = NULL;

    m_pOutputResource = NULL;
    m_pOutputView = NULL;

    m_cameraFGC.pFGSegment = 0;
    m_camera3DLUT17        = 0;
    m_camera3DLUT33        = 0;
    m_camera3DLUT65        = 0;

    m_cachedReadyTaskIndex.clear();

    m_file = 0;
    m_core = 0;

    m_bCameraMode = false;
    m_eventHandle = 0;
    m_bUseEventHandle = false;
} // D3D11VideoProcessor::D3D11VideoProcessor(ID3D11VideoDevice  *pVideoDevice, ID3D11VideoContext *pVideoContext)


D3D11VideoProcessor::~D3D11VideoProcessor(void)
{
    Close();

} // D3D11VideoProcessor::~D3D11VideoProcessor(void)


mfxStatus D3D11VideoProcessor::Init(
                                    ID3D11Device *pDevice,
                                    ID3D11VideoDevice *pVideoDevice,
                                    ID3D11DeviceContext *pDeviceContext,
                                    ID3D11VideoContext *pVideoContext,
                                    mfxVideoParam* par)
{

    mfxStatus sts = MFX_ERR_NONE;

    if (NULL == pVideoDevice || NULL == pVideoContext)
    {
        return MFX_ERR_NULL_PTR;
    }

    m_video = *par;

    m_pDevice = pDevice;
    m_pVideoDevice = pVideoDevice;

    m_pDeviceContext = pDeviceContext;
    m_pVideoContext = pVideoContext;
    m_videoProcessorStreams.clear();
    m_videoProcessorInputViews.clear();
    m_videoProcessorOutputViews.clear();

    // [ reset all states ]
    memset(&m_varianceCaps, 0, sizeof(PREPROC_QUERY_VARIANCE_CAPS));
    memset( &m_frcState, 0, sizeof(D3D11Frc));

    // [ configuration ]
    HRESULT hRes;

    m_videoProcessorDescr.InputFrameFormat = D3D11PictureStructureMapping(par->vpp.In.PicStruct);

    m_videoProcessorDescr.InputFrameRate = D3D11FrameRateMapping(par->vpp.In.FrameRateExtD,   par->vpp.In.FrameRateExtN);
    m_videoProcessorDescr.OutputFrameRate = D3D11FrameRateMapping(par->vpp.Out.FrameRateExtD, par->vpp.Out.FrameRateExtN);

    m_videoProcessorDescr.InputWidth = par->vpp.In.Width;
    m_videoProcessorDescr.InputHeight = par->vpp.In.Height;
    m_videoProcessorDescr.OutputWidth = 1920;par->vpp.Out.Width;
    m_videoProcessorDescr.OutputHeight = 1080;par->vpp.Out.Height;
    m_videoProcessorDescr.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    // create video processor enumerator
    hRes = m_pVideoDevice->CreateVideoProcessorEnumerator(
        &m_videoProcessorDescr,
        &m_pVideoProcessorEnum);
    CHECK_HRES(hRes);

    QueryVideoProcessorCaps();

    // create video processor device
    hRes = m_pVideoDevice->CreateVideoProcessor(
        m_pVideoProcessorEnum,
        0,
        &m_pVideoProcessor);
    CHECK_HRES(hRes);

    {
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC descr;

        m_pVideoProcessor->GetContentDesc(&descr);

#ifdef DEBUG_DETAIL_INFO
        char cStr[512];

        sprintf_s(cStr, "DESCRIPTION\n\
                        InputFrameFormat: %d\n\
                        InputFrameRate: %d %d\n\
                        InputWidth: %d InputHeight: %d \n\
                        OutputFrameRate: %d %d \n\
                        OutputWidth: %d OutputHeight: %d \n\
                        Usage: %d\n",
                        descr.InputFrameFormat,
                        descr.InputFrameRate.Numerator,
                        descr.InputFrameRate.Denominator,
                        descr.InputWidth,
                        descr.InputHeight,
                        descr.OutputFrameRate.Numerator,
                        descr.OutputFrameRate.Denominator,
                        descr.OutputWidth,
                        descr.OutputHeight,
                        descr.Usage);

        OutputDebugStringA(cStr);
#endif
    }

    {
        D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rateConvCaps;

        m_pVideoProcessor->GetRateConversionCaps(&rateConvCaps);

        if (rateConvCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB)
        {
            m_caps.m_simpleDIEnable = true;
        }

        if (rateConvCaps.ProcessorCaps & (D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_MOTION_COMPENSATION | D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE))
        {
            m_caps.m_advancedDIEnable = true;
        }

#ifdef DEBUG_DETAIL_INFO
        char cStr[256];

        sprintf_s(cStr, "   RATE CONV CAPS\n\
                        PastFrames: %d\n\
                        FutureFrames: %d\n\
                        ProcessorCaps: %d \n\
                        ITelecineCaps: %d \n\
                        CustomRateCount: %d \n",
                        rateConvCaps.PastFrames,
                        rateConvCaps.FutureFrames,
                        rateConvCaps.ProcessorCaps,
                        rateConvCaps.ITelecineCaps,
                        rateConvCaps.CustomRateCount);

        OutputDebugStringA(cStr);
#endif
    }

    {
        UINT formatFlag;

        for (int i = 0; i < 150; i++)
        {
            m_pVideoProcessorEnum->CheckVideoProcessorFormat((DXGI_FORMAT)i, &formatFlag);
            m_caps.m_formatSupport[(DXGI_FORMAT)i] = formatFlag;
        }
    }

    sts = QueryVPE_AndExtCaps();
    MFX_CHECK_STS(sts);

    if (m_vpreCaps.bVariance)
    {
        sts = QueryVarianceCaps(m_varianceCaps);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoProcessor::Init(D3D11_VIDEO_FRAME_FORMAT srcPicStruct, IppiSize dstSize, IppiSize srcSize, DXGI_RATIONAL dstFrm, DXGI_RATIONAL srcFrm)


mfxStatus D3D11VideoProcessor::ReconfigDevice(mfxU32 indx)
{
    if(0 == indx ) return MFX_ERR_NONE;

    SAFE_RELEASE(m_pVideoProcessor);

    HRESULT hRes = m_pVideoDevice->CreateVideoProcessor(
        m_pVideoProcessorEnum,
        indx,
        &m_pVideoProcessor);
    CHECK_HRES(hRes);

    mfxStatus sts = QueryVPE_AndExtCaps();
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoProcessor::ReconfigDevice(mfxU32 indx)


mfxStatus D3D11VideoProcessor::QueryVPE_AndExtCaps(void)
{
    HRESULT hRes = S_OK;

    VPE_GUID_ENUM guidEnum;

    guidEnum.Reserved   = 0;
    guidEnum.AllocSize  = 0;
    guidEnum.GuidCount  = 0;
    guidEnum.pGuidArray = NULL;

    hRes = GetOutputExtension(
        &VPE_GUID,
        sizeof(VPE_GUID_ENUM),
        &guidEnum);
    CHECK_HRES(hRes);

    std::vector<mfxU8> guidArr( guidEnum.AllocSize + 24 );//+24 is wa only. should be fixed on VPG side ASAP

    guidEnum.pGuidArray = (VPE_GUID_INFO*)&guidArr[0];

    if (NULL == guidEnum.pGuidArray)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    hRes = GetOutputExtension(
        &VPE_GUID,
        sizeof(VPE_GUID_ENUM),
        &guidEnum);
    CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
    for (mfxU16 guidIdx = 0; guidIdx < guidEnum.GuidCount; guidIdx++)
    {
        VPE_GUID_INFO *pGuidInfo = &guidEnum.pGuidArray[guidIdx];

        printf("GUID[%d] = ", guidIdx);
        printGuid(pGuidInfo->Guid);

        for(mfxU16 verIdx = 0; verIdx < pGuidInfo->VersionCount; verIdx++)
        {
            printf("\n\tVersion = %04x\n", pGuidInfo->pVersion[verIdx]);
        }
    }
    printf("\n");
#endif

    bool isVer2Enable = false;
    mfxU16 idxVer2 = 0;
    bool isVer1Enable = false;
    mfxU16 idxVer1 = 0;

    for (mfxU16 guidIdx = 0; guidIdx < guidEnum.GuidCount; guidIdx++)
    {
        VPE_GUID_INFO *pGuidInfo = &guidEnum.pGuidArray[guidIdx];

#ifdef DEBUG_DETAIL_INFO
        printf("GUID[%d] = ", guidIdx);
        printGuid(pGuidInfo->Guid);
#endif

        if(VPE_GUID_INTERFACE_V1 == pGuidInfo->Guid)
        {
            isVer1Enable = true;
            idxVer1      = guidIdx;
        }

        if(VPE_GUID_INTERFACE_V2 == pGuidInfo->Guid)
        {
            isVer2Enable = true;
            idxVer2      = guidIdx;
        }

#ifdef DEBUG_DETAIL_INFO
        for(mfxU16 verIdx = 0; verIdx < pGuidInfo->VersionCount; verIdx++)
        {
            printf("\n\tVersion = %04x\n", pGuidInfo->pVersion[verIdx]);
        }
        if( 0 == pGuidInfo->VersionCount )
        {
             printf("\n\tVersion = N/A\n");
        }
        printf("\n");
#endif
    }

    // Check v1 guid first to force 15.31 and 15.33 drivers use it
    // 15.36 shouldn't report v1 guid and switches to v2 guid
    if(isVer1Enable)
    {
        m_iface.guid    = guidEnum.pGuidArray[idxVer1].Guid;
        m_iface.version = guidEnum.pGuidArray[idxVer1].pVersion[0];
    }
    else if(isVer2Enable)
    {
        m_iface.guid    = guidEnum.pGuidArray[idxVer2].Guid;
        m_iface.version = guidEnum.pGuidArray[idxVer2].pVersion[0];
    }
    else
    {
        m_iface.guid    = guidEnum.pGuidArray[0].Guid;       // GUID {4E61148B-46D7-4179-81AA-1163EE8EA1E9}
        m_iface.version = guidEnum.pGuidArray[0].pVersion[0];// 0x0000
    }


    //-----------------------------------------------------

    if( VPE_GUID_INTERFACE_V2  != m_iface.guid )
    {
        PREPROC_MODE preprocMode;
        memset(&preprocMode, 0, sizeof(PREPROC_MODE));
        memset(&m_vpreCaps, 0, sizeof(PREPROC_QUERYCAPS));

        preprocMode.Version           = m_iface.version;
        preprocMode.Function          = VPE_FN_QUERY_PREPROC_CAPS;
        preprocMode.pPreprocQueryCaps = &m_vpreCaps;

        hRes = GetOutputExtension(
            &(m_iface.guid),
            sizeof(PREPROC_MODE),
            &preprocMode);
        CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
        printf("Success: get preproc caps\n");

        printf("bInterlacedScaling  = [%d]\n", m_vpreCaps.bInterlacedScaling);
        printf("bSceneDetection     = [%d]\n", m_vpreCaps.bSceneDetection);
        printf("bTargetSysMemory    = [%d]\n", m_vpreCaps.bTargetSysMemory);
        printf("bIS                 = [%d]\n", m_vpreCaps.bIS);
        printf("bVariance           = [%d]\n", m_vpreCaps.bVariance);
        printf("bFieldWeavingControl= [%d]\n", m_vpreCaps.bFieldWeavingControl);
#endif
    }
    else //if( VPE_GUID_INTERFACE_V2  == m_iface.guid )
    {
        VPE_FUNCTION iFunc;

        VPE_VERSION  version = {m_iface.version};
        iFunc.Function = VPE_FN_SET_VERSION_PARAM;
        iFunc.pVersion = &version;

        hRes = SetOutputExtension(
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
        CHECK_HRES(hRes);


        VPE_SET_STATUS_PARAM statusParam = {1, 0};
        iFunc.Function        = VPE_FN_SET_STATUS_PARAM;
        iFunc.pSetStatusParam = &statusParam;

        hRes = SetOutputExtension(
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
        CHECK_HRES(hRes);


        VPE_MODE_PARAM modeParam = {VPE_MODE_PREPROC};
        iFunc.Function   = VPE_FN_MODE_PARAM;
        iFunc.pModeParam = &modeParam;

        hRes = SetOutputExtension(
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
        CHECK_HRES(hRes);


        VPE_VPREP_CAPS vpeCaps = {0};
        iFunc.Function   = VPE_FN_VPREP_QUERY_CAPS;
        iFunc.pVprepCaps = &vpeCaps;

        hRes = GetOutputExtension(
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
        CHECK_HRES(hRes);
        // copy to old structure
        memset(&m_vpreCaps, 0, sizeof(PREPROC_QUERYCAPS));
        m_vpreCaps.bInterlacedScaling   = vpeCaps.bInterlacedScaling;
        m_vpreCaps.bIS                  = vpeCaps.bImageStabilization;
        m_vpreCaps.bFieldWeavingControl = vpeCaps.bFieldWeavingControl;
        m_vpreCaps.bVariance            = vpeCaps.bVariances;
        m_vpreCaps.bScalingMode         = vpeCaps.bScalingMode;
#ifdef DEBUG_DETAIL_INFO
        printf("Success: VPE VPREP CAPS\n");

        printf("bImageStabilization  = [%d]\n", vpeCaps.bImageStabilization);
        printf("bInterlacedScaling   = [%d]\n", vpeCaps.bInterlacedScaling);
        printf("bFieldWeavingControl = [%d]\n", vpeCaps.bFieldWeavingControl);
        printf("bVariances           = [%d]\n", vpeCaps.bVariances);

        // copy to old structure


        // to prevent any issues (HLD not completed yet on VPG side)
        //m_vpreCaps.bVariance = 0;
#endif

        // Camera Pipe caps
        CAMPIPE_MODE cameraPipe;
        memset(&cameraPipe, 0, sizeof(cameraPipe));
        memset(&m_cameraCaps, 0, sizeof(VPE_CP_CAPS));
        cameraPipe.Function = VPE_FN_CP_QUERY_CAPS;
        cameraPipe.pCPCaps  = &m_cameraCaps;
        hRes = GetOutputExtension(
            &(m_iface.guid),
            sizeof(CAMPIPE_MODE),
            &cameraPipe);
        if ( ! FAILED(hRes) )
        {
            // Must have call. If not called, camera pipe may crash driver.
            CAPS_EXTENSION CamPipeCAPSextn ;
            memset((PVOID) &CamPipeCAPSextn, 0, sizeof(CamPipeCAPSextn));
            hRes = GetExtensionCaps(m_pDevice, &CamPipeCAPSextn);

            //Prepare array for gamma correction
            if ( m_cameraCaps.bForwardGamma && m_cameraCaps.SegEntryForwardGamma > 0 )
                m_cameraFGC.pFGSegment = new CP_FORWARD_GAMMA_SEG[m_cameraCaps.SegEntryForwardGamma];
            if ( m_cameraCaps.b3DLUT )
            {
                // Allocate memory for 3DLUT tables
                m_camera3DLUT17 = (LUT17 *)malloc(sizeof(LUT17));
                MFX_CHECK_NULL_PTR1(m_camera3DLUT17);
                memset(m_camera3DLUT17, 0, sizeof(LUT17));

                m_camera3DLUT33 = (LUT33 *)malloc(sizeof(LUT33));
                MFX_CHECK_NULL_PTR1(m_camera3DLUT33);
                memset(m_camera3DLUT33, 0, sizeof(LUT33));

                m_camera3DLUT65 = (LUT65 *)malloc(sizeof(LUT65));
                MFX_CHECK_NULL_PTR1(m_camera3DLUT65);
                memset(m_camera3DLUT65, 0, sizeof(LUT65));
            }
        }
        else
        {
            // Requesting camera related caps returns fail on non-supported platforms. Just ignore this. 
        }

#ifdef DEBUG_DETAIL_INFO
        printf("Success: VPE CAMERA CAPS\n");
        printf("bBlackLevelCorrection     = [%d]\n", m_cameraCaps.bBlackLevelCorrection);
        printf("bColorCorrection          = [%d]\n", m_cameraCaps.bColorCorrection);
        printf("bForwardGamma             = [%d]\n", m_cameraCaps.bForwardGamma);
        printf("bHotPixel                 = [%d]\n", m_cameraCaps.bHotPixel);
        printf("bLensDistortionCorrection = [%d]\n", m_cameraCaps.bLensDistortionCorrection);
        printf("bRgbToYuvCSC              = [%d]\n", m_cameraCaps.bRgbToYuvCSC);
        printf("bVignetteCorrection       = [%d]\n", m_cameraCaps.bVignetteCorrection);
        printf("bWhiteBalanceAuto         = [%d]\n", m_cameraCaps.bWhiteBalanceAuto);
        printf("bWhiteBalanceManual       = [%d]\n", m_cameraCaps.bWhiteBalanceManual);
        printf("bYuvToRgbCSC              = [%d]\n", m_cameraCaps.bYuvToRgbCSC);
#endif
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoProcessor::QueryVPE_AndExtCaps(void)

mfxStatus D3D11VideoProcessor::Close()
{
    m_cachedReadyTaskIndex.clear();

    std::map<void *, D3D11_VIDEO_PROCESSOR_STREAM *>::iterator itVPS;
    for (itVPS = m_videoProcessorStreams.begin() ; itVPS != m_videoProcessorStreams.end(); itVPS++)
    {
        SAFE_DELETE(itVPS->second);
    }

    std::map<void *, ID3D11VideoProcessorInputView *>::iterator itVPIV;
    for (itVPIV = m_videoProcessorInputViews.begin() ; itVPIV != m_videoProcessorInputViews.end(); itVPIV++)
    {
        SAFE_RELEASE(itVPIV->second);
    }

    std::map<void *, ID3D11VideoProcessorOutputView *>::iterator itVPOV;
    for (itVPOV = m_videoProcessorOutputViews.begin() ; itVPOV != m_videoProcessorOutputViews.end(); itVPOV++)
    {
        SAFE_RELEASE(itVPOV->second);
    }

    for(size_t refIdx = 0; refIdx > m_pInputView.size(); refIdx++ )
    {
        SAFE_RELEASE(m_pInputView[refIdx]);
    }
    m_pInputView.clear();

    SAFE_RELEASE(m_pOutputView);

    SAFE_RELEASE(m_pVideoProcessor);
    SAFE_RELEASE(m_pVideoProcessorEnum);

    // FRC
    memset( &m_frcState, 0, sizeof(D3D11Frc));

    //
    if(m_file)
    {
        vm_file_fclose(m_file);
        m_file = 0;
    }
    if (m_cameraFGC.pFGSegment)
        delete [] m_cameraFGC.pFGSegment;

    if (m_camera3DLUT17)
        free(m_camera3DLUT17);

    if(m_camera3DLUT33)
        free(m_camera3DLUT33);

    if(m_camera3DLUT65)
        free(m_camera3DLUT65);

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoProcessor::Close()


mfxStatus D3D11VideoProcessor::QueryVarianceCaps(PREPROC_QUERY_VARIANCE_CAPS& varianceCaps)
{
    HRESULT hRes = S_OK;

    if( VPE_GUID_INTERFACE_V2  != m_iface.guid )
    {
        PREPROC_MODE preprocMode;
        memset(&preprocMode,    0, sizeof(PREPROC_MODE));
        memset(&varianceCaps, 0, sizeof(PREPROC_QUERY_VARIANCE_CAPS));

        preprocMode.Version           = m_iface.version;
        preprocMode.Function          = VPE_FN_QUERY_VARIANCE_CAPS;
        preprocMode.pVarianceCaps      = &varianceCaps;

        hRes = GetOutputExtension(
            &(m_iface.guid),
            sizeof(PREPROC_MODE),
            &preprocMode);
    }
    else //if( VPE_GUID_INTERFACE_V2  == m_iface.guid )
    {
        VPE_FUNCTION iFunc = {0};
        //VPE_VERSION  version = {m_iface.version};

        VPE_VPREP_VARIANCE_CAPS vpeVarianceCaps;
        memset(&vpeVarianceCaps, 0, sizeof(VPE_VPREP_VARIANCE_CAPS));
        iFunc.Function   = VPE_FN_VPREP_QUERY_VARIANCE_CAPS;
        //iFunc.pVersion = &version; // no need to set used version?
        iFunc.pVarianceCaps = &vpeVarianceCaps;

        hRes = GetOutputExtension(
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);

        // copy to old structure
        //varianceCaps.Type = vpeVarianceCaps.Type;
        varianceCaps.VarianceCount = vpeVarianceCaps.VarianceCount;
        varianceCaps.VarianceSize = vpeVarianceCaps.VarianceSize;
    }

    CHECK_HRES(hRes);

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoProcessor::QueryVarianceCaps(PREPROC_QUERY_VARIANCE_CAPS& varianceCaps)


mfxStatus D3D11VideoProcessor::QueryVideoProcessorCaps(void)
{
    HRESULT hRes;
    D3D11_VIDEO_PROCESSOR_CAPS videoProcessorCaps;

    hRes = m_pVideoProcessorEnum->GetVideoProcessorCaps(&videoProcessorCaps);
    CHECK_HRES(hRes);

    {
#ifdef DEBUG_DETAIL_INFO
        char cStr[256];

        sprintf_s(cStr, "D3D11_VIDEO_PROCESSOR_DEVICE_CAPS:\n");
        OutputDebugStringA(cStr);

        sprintf_s(cStr, "\tRateConversionCapsCount: %d\n\tMaxInputStreams: %d\n\tMaxStreamStates: %d\n",
            videoProcessorCaps.RateConversionCapsCount,
            videoProcessorCaps.MaxInputStreams,
            videoProcessorCaps.MaxStreamStates);

        OutputDebugStringA(cStr);
#endif

        if (D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_ROTATION & videoProcessorCaps.FeatureCaps)
        {
            m_caps.m_rotation = true;
        }

        D3D11_VIDEO_PROCESSOR_FILTER_RANGE range;
        const mfxF32 EPSILA = 0.000000001f;

        if (D3D11_VIDEO_PROCESSOR_FILTER_CAPS_BRIGHTNESS & videoProcessorCaps.FilterCaps)
        {
            hRes = m_pVideoProcessorEnum->GetVideoProcessorFilterRange(D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS, &range);
            CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
            sprintf_s(cStr, "\tD3D11_VIDEO_PROCESSOR_FILTER_CAPS_BRIGHTNESS: %d - %d, %d, %f\n", range.Minimum, range.Maximum, range.Default, range.Multiplier);
            OutputDebugStringA(cStr);
#endif
            if( fabs(range.Multiplier) > EPSILA )   m_multiplier.brightness = 1.f / range.Multiplier;

            m_caps.m_procAmpEnable = true;
        }

        if (D3D11_VIDEO_PROCESSOR_FILTER_CAPS_CONTRAST & videoProcessorCaps.FilterCaps)
        {
            hRes = m_pVideoProcessorEnum->GetVideoProcessorFilterRange(D3D11_VIDEO_PROCESSOR_FILTER_CONTRAST, &range);
            CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
            sprintf_s(cStr, "\tD3D11_VIDEO_PROCESSOR_FILTER_CAPS_CONTRAST: %d - %d, %d, %f\n", range.Minimum, range.Maximum, range.Default, range.Multiplier);
            OutputDebugStringA(cStr);
#endif
            if( fabs(range.Multiplier) > EPSILA )   m_multiplier.contrast = 1.f / range.Multiplier;

            m_caps.m_procAmpEnable = true;
        }

        if (D3D11_VIDEO_PROCESSOR_FILTER_CAPS_HUE & videoProcessorCaps.FilterCaps)
        {
            hRes = m_pVideoProcessorEnum->GetVideoProcessorFilterRange(D3D11_VIDEO_PROCESSOR_FILTER_HUE, &range);
            CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
            sprintf_s(cStr, "\tD3D11_VIDEO_PROCESSOR_FILTER_CAPS_HUE: %d - %d, %d, %f\n", range.Minimum, range.Maximum, range.Default, range.Multiplier);
            OutputDebugStringA(cStr);
#endif
            if( fabs(range.Multiplier) > EPSILA )   m_multiplier.hue = 1.f / range.Multiplier;

            m_caps.m_procAmpEnable = true;
        }

        if (D3D11_VIDEO_PROCESSOR_FILTER_CAPS_SATURATION & videoProcessorCaps.FilterCaps)
        {
            hRes = m_pVideoProcessorEnum->GetVideoProcessorFilterRange(D3D11_VIDEO_PROCESSOR_FILTER_SATURATION, &range);
            CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
            sprintf_s(cStr, "\tD3D11_VIDEO_PROCESSOR_FILTER_CAPS_SATURATION: %d - %d, %d, %f\n", range.Minimum, range.Maximum, range.Default, range.Multiplier);
            OutputDebugStringA(cStr);
#endif
            if( fabs(range.Multiplier) > EPSILA )   m_multiplier.saturation = 1.f / range.Multiplier;

            m_caps.m_procAmpEnable = true;
        }

        if (D3D11_VIDEO_PROCESSOR_FILTER_CAPS_NOISE_REDUCTION & videoProcessorCaps.FilterCaps)
        {
            hRes = m_pVideoProcessorEnum->GetVideoProcessorFilterRange(D3D11_VIDEO_PROCESSOR_FILTER_NOISE_REDUCTION, &range);
            CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
            sprintf_s(cStr, "\tD3D11_VIDEO_PROCESSOR_FILTER_CAPS_NOISE_REDUCTION: %d - %d, %d, %f\n", range.Minimum, range.Maximum, range.Default, range.Multiplier);
            OutputDebugStringA(cStr);
#endif

            m_caps.m_denoiseEnable = true;
        }

        if (D3D11_VIDEO_PROCESSOR_FILTER_CAPS_EDGE_ENHANCEMENT & videoProcessorCaps.FilterCaps)
        {

            hRes = m_pVideoProcessorEnum->GetVideoProcessorFilterRange(D3D11_VIDEO_PROCESSOR_FILTER_EDGE_ENHANCEMENT, &range);
            CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
            sprintf_s(cStr, "\tD3D11_VIDEO_PROCESSOR_FILTER_CAPS_EDGE_ENHANCEMENT: %d - %d, %d, %f\n", range.Minimum, range.Maximum, range.Default, range.Multiplier);
            OutputDebugStringA(cStr);
#endif
            m_caps.m_detailEnable = true;
        }
    }

    //-----------------------------------------------------
    // FRC start
    D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS  videoProcessorRateConvCaps;
    for(mfxU32 rateIndx = 0; rateIndx < videoProcessorCaps.RateConversionCapsCount; rateIndx++)
    {
        hRes = m_pVideoProcessorEnum->GetVideoProcessorRateConversionCaps(rateIndx, &videoProcessorRateConvCaps);
        CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
        char cStr[256];
        sprintf_s(cStr, "D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS:\n");
        OutputDebugStringA(cStr);

        sprintf_s(cStr, "\tCustomRateCount: %d\n\tFutureFrames: %d\n\tPastFrames: %d\n",
            videoProcessorRateConvCaps.CustomRateCount,
            videoProcessorRateConvCaps.FutureFrames,
            videoProcessorRateConvCaps.PastFrames);

        OutputDebugStringA(cStr);

#endif


        D3D11_VIDEO_PROCESSOR_CUSTOM_RATE           videoProcessorCustomRate;
        for(mfxU32 customRateIndx = 0; customRateIndx < videoProcessorRateConvCaps.CustomRateCount; customRateIndx++)
        {
            hRes = m_pVideoProcessorEnum->GetVideoProcessorCustomRate(rateIndx, customRateIndx, &videoProcessorCustomRate);
            CHECK_HRES(hRes);

            CustomRateData curRateData;
            curRateData.customRate.FrameRateExtN = videoProcessorCustomRate.CustomRate.Numerator;
            curRateData.customRate.FrameRateExtD = videoProcessorCustomRate.CustomRate.Denominator;
            curRateData.inputFramesOrFieldPerCycle= videoProcessorCustomRate.InputFramesOrFields;
            curRateData.outputIndexCountPerCycle  = videoProcessorCustomRate.OutputFrames;
            curRateData.fwdRefCount       = videoProcessorRateConvCaps.FutureFrames;
            curRateData.bkwdRefCount      = videoProcessorRateConvCaps.PastFrames;

            curRateData.indexRateConversion = rateIndx;

            m_customRateData.push_back(curRateData);
        }
    }// FRC stop
    //-----------------------------------------------------

    UINT flag;
    DXGI_FORMAT queryFormats[] = {
        DXGI_FORMAT_R16_TYPELESS,
        DXGI_FORMAT_NV12,
        DXGI_FORMAT_YUY2,
        DXGI_FORMAT_420_OPAQUE,
        DXGI_FORMAT_B8G8R8A8_UNORM};

        size_t formatCount = sizeof(queryFormats) / sizeof(queryFormats[0]);

        for (mfxU32 i = 0; i < formatCount; i++)
        {
            hRes = m_pVideoProcessorEnum->CheckVideoProcessorFormat(queryFormats[i], &flag);
            CHECK_HRES(hRes);

#ifdef DEBUG_DETAIL_INFO
            char cStr[128];

            if (D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT & flag)
            {
                sprintf_s(cStr, "\t%d is supported as input\n", queryFormats[i]);
                OutputDebugStringA(cStr);
            }

            if (D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT & flag)
            {
                sprintf_s(cStr, "\t%d is supported as output\n", queryFormats[i]);
                OutputDebugStringA(cStr);
            }
#endif
        }

        return MFX_ERR_NONE;

} // mfxStatus D3D11VideoProcessor::QueryVideoProcessorCaps(void)

//void D3D11VideoProcessor::GetVideoProcessorCapabilities(D3D11_VIDEO_PROCESSOR_CAPS & videoProcessorCaps_, D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS & videoProcessorRateConvCaps_)
//{
//    videoProcessorCaps_ = m_videoProcessorCaps;
//    videoProcessorRateConvCaps_ = m_videoProcessorRateConvCaps;
//
//} // mfxStatus D3D11VideoProcessor::GetVideoProcessorCapabilities(D3D11_VIDEO_PROCESSOR_CAPS m_videoProcessorCaps, D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS m_videoProcessorRateConvCaps)

void D3D11VideoProcessor::SetOutputTargetRect(BOOL Enable, RECT *pRect)
{

    m_pVideoContext->VideoProcessorSetOutputTargetRect(m_pVideoProcessor, Enable, pRect);

} // void D3D11VideoProcessor::SetOutputTargetRect(BOOL Enable, RECT *pRect)

void D3D11VideoProcessor::GetOutputTargetRect(BOOL *Enabled, RECT *pRect)
{

    m_pVideoContext->VideoProcessorGetOutputTargetRect(m_pVideoProcessor, Enabled, pRect);

} // void D3D11VideoProcessor::GetOutputTargetRect(BOOL *Enabled, RECT *pRect)

void D3D11VideoProcessor::SetOutputBackgroundColor(BOOL YCbCr, D3D11_VIDEO_COLOR *pColor)
{

    m_pVideoContext->VideoProcessorSetOutputBackgroundColor(m_pVideoProcessor, YCbCr, pColor);

} // void D3D11VideoProcessor::SetOutputBackgroundColor(BOOL YCbCr, D3D11_VIDEO_COLOR *pColor)

void D3D11VideoProcessor::GetOutputBackgroundColor(BOOL *pYCbCr, D3D11_VIDEO_COLOR *pColor)
{

    m_pVideoContext->VideoProcessorGetOutputBackgroundColor(m_pVideoProcessor, pYCbCr, pColor);

} // void D3D11VideoProcessor::GetOutputBackgroundColor(BOOL *pYCbCr, D3D11_VIDEO_COLOR *pColor)

void D3D11VideoProcessor::SetOutputColorSpace(D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
{

    m_pVideoContext->VideoProcessorSetOutputColorSpace(m_pVideoProcessor, pColorSpace);

} // void D3D11VideoProcessor::SetOutputColorSpace(D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)

void D3D11VideoProcessor::GetOutputColorSpace(D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
{

    m_pVideoContext->VideoProcessorGetOutputColorSpace(m_pVideoProcessor, pColorSpace);

} // void D3D11VideoProcessor::GetOutputColorSpace(D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)

void D3D11VideoProcessor::SetOutputAlphaFillMode(D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode, UINT StreamIndex)
{

    m_pVideoContext->VideoProcessorSetOutputAlphaFillMode(m_pVideoProcessor, AlphaFillMode, StreamIndex);

} // void D3D11VideoProcessor::SetOutputAlphaFillMode(D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode, UINT StreamIndex)

void D3D11VideoProcessor::GetOutputAlphaFillMode(D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE *pAlphaFillMode, UINT *pStreamIndex)
{

    m_pVideoContext->VideoProcessorGetOutputAlphaFillMode(m_pVideoProcessor, pAlphaFillMode, pStreamIndex);

} // void D3D11VideoProcessor::GetOutputAlphaFillMode(D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE *pAlphaFillMode, UINT *pStreamIndex)

void D3D11VideoProcessor::SetOutputConstriction(BOOL Enable, SIZE Size)
{

    m_pVideoContext->VideoProcessorSetOutputConstriction(m_pVideoProcessor, Enable, Size);

} // void D3D11VideoProcessor::SetOutputConstriction(BOOL Enable, SIZE Size)

void D3D11VideoProcessor::GetOutputConstriction(BOOL *pEnabled, SIZE *pSize)
{

    m_pVideoContext->VideoProcessorGetOutputConstriction(m_pVideoProcessor, pEnabled, pSize);

} // void D3D11VideoProcessor::GetOutputConstriction(BOOL *pEnabled, SIZE *pSize)

HRESULT D3D11VideoProcessor::SetOutputExtension(const GUID* pExtensionGuid, UINT DataSize, void* pData)
{

    HRESULT hRes;

//#ifdef DEBUG_DETAIL_INFO
//    printf("\n----------\n");fflush(stderr);
//    printf("State BEFORE  SetOutputExtension()  call\n");fflush(stderr);
//    printf("pVideoProcessor = %p\n", m_pVideoProcessor);fflush(stderr);
//    printf("Guid  = ");printGuid(*pExtensionGuid);printf("\n");fflush(stderr);
//    printf("DataSize        = %i\n", DataSize);fflush(stderr);
//    printf("pData           = %p\n", pData);fflush(stderr);
//
//    PREPROC_MODE *pMode = (PREPROC_MODE*)pData;
//    printf("Function = %i\n", pMode->Function);fflush(stderr);
//    printf("Version  = %i\n", pMode->Version);fflush(stderr);
//
////    enum
////{
////    VPE_FN_QUERY_PREPROC_CAPS     = 0x0000,
////    VPE_FN_SET_PREPROC_MODE       = 0x0001,
////    VPE_FN_QUERY_SCENE_DETECTION  = 0x0002,
////    VPE_FN_QUERY_PREPROC_STATUS   = 0x0003,
////    VPE_FN_SET_IS_PARAMS          = 0x0004,
////    VPE_FN_QUERY_VARIANCE_CAPS    = 0x0005,
////    VPE_FN_QUERY_VARIANCE         = 0x0005,
////};
//    if(pMode->Function == VPE_FN_SET_PREPROC_MODE)
//    {
//        SET_PREPROC_PARAMS *pParams = pMode->pPreprocParams;
//        printf("bEnablePreProcMode     = %i\n", pParams->bEnablePreProcMode) ;fflush(stderr);
//        printf("iTargetInterlacingMode = %i\n", pParams->iTargetInterlacingMode) ;fflush(stderr);
//        printf("bTargetInSysMemory     = %i\n", pParams->bTargetInSysMemory);fflush(stderr);
//        printf("bSceneDetectionEnable  = %i\n", pParams->bSceneDetectionEnable);fflush(stderr);
//        printf("bVarianceQuery         = %i\n", pParams->bVarianceQuery);fflush(stderr);
//        printf("bFieldWeaving          = %i\n", pParams->bFieldWeaving);fflush(stderr);
//        //printf("Reserved               : 26;
//        printf("StatusReportID         = %i\n", pParams->StatusReportID);fflush(stderr);
//    }
//    else if(pMode->Function == VPE_FN_QUERY_VARIANCE)
//    {
//        PREPROC_QUERY_VARIANCE_PARAMS *pParams = pMode->pVarianceParams;
//        printf("FrameNumber         = %i\n", pParams->FrameNumber) ;fflush(stderr);
//        printf("pVariances          = %p\n", pParams->pVariances) ;fflush(stderr);
//        printf("VarianceBufferSize  = %p\n", pParams->VarianceBufferSize) ;fflush(stderr);
//    }
//
//
//#endif

    hRes = m_pVideoContext->VideoProcessorSetOutputExtension(
        m_pVideoProcessor,
        pExtensionGuid,
        DataSize,
        pData);

#ifdef DEBUG_DETAIL_INFO
    printf("\n sts = %i \n", hRes);fflush(stderr);
#endif

    return hRes;

} // HRESULT D3D11VideoProcessor::SetOutputExtension(const GUID* pExtensionGuid, UINT DataSize, void* pData)

void D3D11VideoProcessor::SetStreamFrameFormat(UINT StreamIndex, D3D11_VIDEO_FRAME_FORMAT FrameFormat)
{

    m_pVideoContext->VideoProcessorSetStreamFrameFormat(m_pVideoProcessor, StreamIndex, FrameFormat);

} // void D3D11VideoProcessor::SetStreamFrameFormat(UINT StreamIndex, D3D11_VIDEO_FRAME_FORMAT FrameFormat)

void D3D11VideoProcessor::SetStreamColorSpace(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace)
{

    m_pVideoContext->VideoProcessorSetStreamColorSpace(m_pVideoProcessor, StreamIndex, pColorSpace);

} // void D3D11VideoProcessor::SetStreamColorSpace(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace)

void D3D11VideoProcessor::SetStreamOutputRate(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE OutputRate, BOOL RepeatFrame, DXGI_RATIONAL* pCustomRate)
{

    m_pVideoContext->VideoProcessorSetStreamOutputRate(m_pVideoProcessor, StreamIndex, OutputRate, RepeatFrame, pCustomRate);

} // void D3D11VideoProcessor::SetStreamOutputRate(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE OutputRate, BOOL RepeatFrame, DXGI_RATIONAL* pCustomRate)

void D3D11VideoProcessor::SetStreamSourceRect(UINT StreamIndex, BOOL Enable, RECT* pRect)
{

    m_pVideoContext->VideoProcessorSetStreamSourceRect(m_pVideoProcessor, StreamIndex, Enable, pRect);

} // void D3D11VideoProcessor::SetStreamSourceRect(UINT StreamIndex, BOOL Enable, RECT* pRect)

void D3D11VideoProcessor::SetStreamDestRect(UINT StreamIndex, BOOL Enable, RECT* pRect)
{

    m_pVideoContext->VideoProcessorSetStreamDestRect(m_pVideoProcessor, StreamIndex, Enable, pRect);

} // void D3D11VideoProcessor::SetStreamDestRect(UINT StreamIndex, BOOL Enable, RECT* pRect)

void D3D11VideoProcessor::SetStreamAlpha(UINT StreamIndex, BOOL Enable, FLOAT Alpha)
{

    m_pVideoContext->VideoProcessorSetStreamAlpha(m_pVideoProcessor, StreamIndex, Enable, Alpha);

} // void D3D11VideoProcessor::SetStreamAlpha(UINT StreamIndex, BOOL Enable, FLOAT Alpha)

void D3D11VideoProcessor::SetStreamLumaKey(UINT StreamIndex, BOOL Enable, FLOAT Lower, FLOAT Upper)
{

    m_pVideoContext->VideoProcessorSetStreamLumaKey(m_pVideoProcessor, StreamIndex, Enable, Lower, Upper);

} // void D3D11VideoProcessor::SetStreamLumaKey(UINT StreamIndex, BOOL Enable, FLOAT Lower, FLOAT Upper)

void D3D11VideoProcessor::SetStreamPalette(UINT StreamIndex, UINT Count, UINT* pEntries)
{

    m_pVideoContext->VideoProcessorSetStreamPalette(m_pVideoProcessor, StreamIndex, Count, pEntries);

} // void D3D11VideoProcessor::SetStreamPalette(UINT StreamIndex, UINT Count, UINT* pEntries)

void D3D11VideoProcessor::SetStreamPixelAspectRatio(UINT StreamIndex, BOOL Enable, DXGI_RATIONAL* pSourceAspectRatio, DXGI_RATIONAL* pDestinationAspectRatio)
{

    m_pVideoContext->VideoProcessorSetStreamPixelAspectRatio(m_pVideoProcessor, StreamIndex, Enable, pSourceAspectRatio, pDestinationAspectRatio);

} // void D3D11VideoProcessor::SetStreamPixelAspectRatio(UINT StreamIndex, BOOL Enable, DXGI_RATIONAL* pSourceAspectRatio, DXGI_RATIONAL* pDestinationAspectRatio)

void D3D11VideoProcessor::SetStreamAutoProcessingMode(UINT StreamIndex, BOOL Enable)
{

    m_pVideoContext->VideoProcessorSetStreamAutoProcessingMode(m_pVideoProcessor, StreamIndex, Enable);

} // void D3D11VideoProcessor::SetStreamAutoProcessingMode(UINT StreamIndex, BOOL Enable)

void D3D11VideoProcessor::SetStreamFilter(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_FILTER Filter, BOOL Enable, int Level)
{

    m_pVideoContext->VideoProcessorSetStreamFilter(m_pVideoProcessor, StreamIndex, Filter, Enable, Level);

} // void D3D11VideoProcessor::SetStreamFilter(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_FILTER Filter, BOOL Enable, int Level)

void D3D11VideoProcessor::SetStreamRotation(UINT StreamIndex, BOOL Enable, int rotation)
{

    m_pVideoContext->VideoProcessorSetStreamRotation(m_pVideoProcessor, StreamIndex, Enable, (D3D11_VIDEO_PROCESSOR_ROTATION)rotation);

}

HRESULT D3D11VideoProcessor::SetStreamExtension(UINT StreamIndex, const GUID* pExtensionGuid, UINT DataSize, void* pData)
{

    HRESULT hRes;

    hRes = m_pVideoContext->VideoProcessorSetStreamExtension(m_pVideoProcessor, StreamIndex, pExtensionGuid, DataSize, pData);

    return hRes;

} // HRESULT D3D11VideoProcessor::SetStreamExtension(const GUID* pExtensionGuid, UINT DataSize, void* pData)

void D3D11VideoProcessor::GetStreamFrameFormat(UINT StreamIndex, D3D11_VIDEO_FRAME_FORMAT* pFrameFormat)
{

    m_pVideoContext->VideoProcessorGetStreamFrameFormat(m_pVideoProcessor, StreamIndex, pFrameFormat);

} // void GetStreamFrameFormat(UINT StreamIndex, D3D11_VIDEO_FRAME_FORMAT* pFrameFormat)

void D3D11VideoProcessor::GetStreamColorSpace(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace)
{

    m_pVideoContext->VideoProcessorGetStreamColorSpace(m_pVideoProcessor, StreamIndex, pColorSpace);

} // void D3D11VideoProcessor::GetStreamColorSpace(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace)

void D3D11VideoProcessor::GetStreamOutputRate(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE* pOutputRate, BOOL* pRepeatFrame, DXGI_RATIONAL* pCustomRate)
{

    m_pVideoContext->VideoProcessorGetStreamOutputRate(m_pVideoProcessor, StreamIndex, pOutputRate, pRepeatFrame, pCustomRate);

} // void D3D11VideoProcessor::GetStreamOutputRate(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE* pOutputRate, BOOL* pRepeatFrame, DXGI_RATIONAL* pCustomRate)

void D3D11VideoProcessor::GetStreamSourceRect(UINT StreamIndex, BOOL* pEnabled, RECT* pRect)
{

    m_pVideoContext->VideoProcessorGetStreamSourceRect(m_pVideoProcessor, StreamIndex, pEnabled, pRect);

} // void D3D11VideoProcessor::GetStreamSourceRect(UINT StreamIndex, BOOL* pEnabled, RECT* pRect)

void D3D11VideoProcessor::GetStreamDestRect(UINT StreamIndex, BOOL* pEnabled, RECT* pRect)
{

    m_pVideoContext->VideoProcessorGetStreamDestRect(m_pVideoProcessor, StreamIndex, pEnabled, pRect);

} // void D3D11VideoProcessor::GetStreamDestRect(UINT StreamIndex, BOOL* pEnabled, RECT* pRect)

void D3D11VideoProcessor::GetStreamAlpha(UINT StreamIndex, BOOL* pEnabled, FLOAT* pAlpha)
{

    m_pVideoContext->VideoProcessorGetStreamAlpha(m_pVideoProcessor, StreamIndex, pEnabled, pAlpha);

} // void D3D11VideoProcessor::GetStreamAlpha(UINT StreamIndex, BOOL* pEnabled, FLOAT* pAlpha)

void D3D11VideoProcessor::GetStreamLumaKey(UINT StreamIndex, BOOL* Enable, FLOAT* Lower, FLOAT* Upper)
{

    m_pVideoContext->VideoProcessorGetStreamLumaKey(m_pVideoProcessor, StreamIndex, Enable, Lower, Upper);

} // void D3D11VideoProcessor::GetStreamLumaKey(UINT StreamIndex, BOOL* Enable, FLOAT* Lower, FLOAT* Upper)

void D3D11VideoProcessor::GetStreamPalette(UINT StreamIndex, UINT Count, UINT* pEntries)
{

    m_pVideoContext->VideoProcessorGetStreamPalette(m_pVideoProcessor, StreamIndex, Count, pEntries);

} // void D3D11VideoProcessor::GetStreamPalette(UINT StreamIndex, UINT Count, UINT* pEntries)

void D3D11VideoProcessor::GetStreamPixelAspectRatio(UINT StreamIndex, BOOL* pEnabled, DXGI_RATIONAL* pSourceAspectRatio, DXGI_RATIONAL* pDestinationAspectRatio)
{

    m_pVideoContext->VideoProcessorGetStreamPixelAspectRatio(m_pVideoProcessor, StreamIndex, pEnabled, pSourceAspectRatio, pDestinationAspectRatio);

} // void D3D11VideoProcessor::GetStreamPixelAspectRatio(UINT StreamIndex, BOOL* pEnabled, DXGI_RATIONAL* pSourceAspectRatio, DXGI_RATIONAL* pDestinationAspectRatio)

void D3D11VideoProcessor::GetStreamAutoProcessingMode(UINT StreamIndex, BOOL* pEnabled)
{

    m_pVideoContext->VideoProcessorGetStreamAutoProcessingMode(m_pVideoProcessor, StreamIndex, pEnabled);

} // void D3D11VideoProcessor::GetStreamAutoProcessingMode(UINT StreamIndex, BOOL* pEnabled)

void D3D11VideoProcessor::GetStreamFilter(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_FILTER Filter, BOOL* pEnabled, int* pLevel)
{

    m_pVideoContext->VideoProcessorGetStreamFilter(m_pVideoProcessor, StreamIndex, Filter, pEnabled, pLevel);

} // void D3D11VideoProcessor::GetStreamFilter(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_FILTER Filter, BOOL* pEnabled, int* pLevel)

HRESULT D3D11VideoProcessor::GetStreamExtension(UINT StreamIndex, const GUID* pExtensionGuid, UINT DataSize, void* pData)
{

    HRESULT hRes;

    hRes = m_pVideoContext->VideoProcessorGetStreamExtension(m_pVideoProcessor, StreamIndex, pExtensionGuid, DataSize, pData);

    return hRes;

} // HRESULT D3D11VideoProcessor::GetStreamExtension(UINT StreamIndex, const GUID* pExtensionGuid, UINT DataSize, void* pData)

HRESULT D3D11VideoProcessor::GetOutputExtension(
    const GUID* pExtensionGuid,
    UINT DataSize,
    void* pData)
{

    HRESULT hRes;

//#ifdef DEBUG_DETAIL_INFO
//    printf("\n----------\n");fflush(stderr);
//    printf("State BEFORE  GetOutputExtension()  call\n");fflush(stderr);
//    printf("pVideoProcessor = %p\n", m_pVideoProcessor);fflush(stderr);
//    printf("Guid            = ");printGuid(*pExtensionGuid);printf("\n");fflush(stderr);
//    printf("DataSize        = %i\n", DataSize);fflush(stderr);
//    printf("pData           = %p\n", pData);fflush(stderr);
//
//    PREPROC_MODE *pMode = (PREPROC_MODE*)pData;
//    printf("Function = %i\n", pMode->Function);fflush(stderr);
//    printf("Version  = %i\n", pMode->Version);fflush(stderr);
//
//    if(pMode->Function == VPE_FN_QUERY_VARIANCE)
//    {
//        PREPROC_QUERY_VARIANCE_PARAMS *pParams = pMode->pVarianceParams;
//        printf("FrameNumber         = %i\n", pParams->FrameNumber) ;fflush(stderr);
//        printf("pVariances          = %p\n", pParams->pVariances) ;fflush(stderr);
//        printf("VarianceBufferSize  = %i\n", pParams->VarianceBufferSize) ;fflush(stderr);
//    }
//
//
//#endif

    hRes = m_pVideoContext->VideoProcessorGetOutputExtension(
        m_pVideoProcessor,
        pExtensionGuid,
        DataSize,
        pData);

#ifdef DEBUG_DETAIL_INFO
    printf("\n sts = %i \n", hRes);fflush(stderr);
#endif

    return hRes;

} // HRESULT D3D11VideoProcessor::GetOutputExtension(const GUID* pExtensionGuid, UINT DataSize, void* pData)

enum QueryStatus
{
    VPREP_GPU_READY         =   0,
    VPREP_GPU_BUSY          =   1,
    VPREP_GPU_NOT_REACHED   =   2,
    VPREP_GPU_FAILED        =   3
};


mfxStatus D3D11VideoProcessor::QueryTaskStatus(mfxU32 idx)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    HRESULT hRes;
    mfxStatus sts;
    if ( m_bUseEventHandle && ! m_eventHandle )
    {
        try
        {
            // EventNotification functionality doesn't have any flags in caps, so
            // the only way to check if it's suuported - try it. 
            sts = GetEventHandle(&m_eventHandle);
            if ( MFX_ERR_NONE != sts )
            {
                m_bUseEventHandle = false;
            }
        }
        catch(...)
        {
            m_bUseEventHandle = false;
            m_eventHandle     = 0;
        }
    }

    if ( m_bUseEventHandle && m_eventHandle )
    {
        DWORD status = WaitForSingleObject(m_eventHandle, 1000);
        switch(status)
        {
        case WAIT_ABANDONED:
        case WAIT_FAILED:
            m_bUseEventHandle = false;
            break;
        case WAIT_TIMEOUT:
            return MFX_TASK_BUSY;
        case WAIT_OBJECT_0:
        default:
            break;
        }
    }

    // aya: the same as for d3d9 (FC)
    const mfxU32 numStructures = 6 * 2;
    std::set<mfxU32>::iterator iterator;

    //-----------------------------------------------------
    if( VPE_GUID_INTERFACE_V2 != m_iface.guid)
    {
        PREPROC_STATUS_QUERY_BUFFER queryBuffer[numStructures];
        PREPROC_STATUS_QUERY_PARAMS queryStatusParams;
        queryStatusParams.StatusReportCount = numStructures;

        memset(&queryBuffer[0], 0, sizeof(PREPROC_STATUS_QUERY_BUFFER) * numStructures);

        queryStatusParams.pStatusBuffer = &queryBuffer[0];

        PREPROC_MODE preprocMode;
        memset(&preprocMode, 0, sizeof(PREPROC_MODE));

    preprocMode.Version  = m_iface.version;
    preprocMode.Function = VPE_FN_QUERY_PREPROC_STATUS;
    preprocMode.pPreprocQueryStatus = &queryStatusParams;
    //to handle TDR, if TDR occured no changes in status will be visible, if not driver will change to corrrect status.
    for (mfxU32 i = 0; i < numStructures; i++)
    {
        queryStatusParams.pStatusBuffer[i].Status = VPREP_GPU_FAILED;
    }
    hRes = GetOutputExtension(
        &(m_iface.guid),
        sizeof(PREPROC_MODE),
            &preprocMode);
        CHECK_HRES(hRes);

        for (mfxU32 i = 0; i < numStructures; i += 1)
        {
            if (VPREP_GPU_READY == queryStatusParams.pStatusBuffer[i].Status)
            {
                m_cachedReadyTaskIndex.insert(queryStatusParams.pStatusBuffer[i].StatusReportID);
            }
            else if (VPREP_GPU_FAILED == queryStatusParams.pStatusBuffer[i].Status)
            {
                return MFX_ERR_DEVICE_FAILED;
            }
        }
    }
    else
    {
        VPE_STATUS_PARAM queryBuffer[numStructures];
        memset(&queryBuffer[0], 0, sizeof(VPE_STATUS_PARAM) * numStructures);

        VPE_GET_STATUS_PARAMS queryParam;
        queryParam.StatusCount   = numStructures;
        queryParam.pStatusBuffer = &queryBuffer[0];

        VPE_FUNCTION iFunc = {0};
        iFunc.Function = VPE_FN_GET_STATUS_PARAM;
        iFunc.pGetStatusParams = &queryParam;

        hRes = GetOutputExtension(
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
        CHECK_HRES(hRes);

        for (mfxU32 i = 0; i < numStructures; i += 1)
        {
            if (VPREP_GPU_READY == queryParam.pStatusBuffer[i].Status)
            {
                m_cachedReadyTaskIndex.insert(queryParam.pStatusBuffer[i].FrameId);
            }
            else if (VPREP_GPU_FAILED == queryParam.pStatusBuffer[i].Status)
            {
                return MFX_ERR_DEVICE_FAILED;
            }
        }
    }
    //-----------------------------------------------------

    iterator = find(m_cachedReadyTaskIndex.begin(), m_cachedReadyTaskIndex.end(), idx);

    if (m_cachedReadyTaskIndex.end() == iterator)
    {
#ifdef DEBUG_DETAIL_INFO
    printf("\n Task with ::StatusID = %i BUSY!!!\n\n", idx);fflush(stderr);
#endif
        return MFX_TASK_BUSY;
    }

    m_cachedReadyTaskIndex.erase(iterator);

#ifdef DEBUG_DETAIL_INFO
    printf("\n Task with ::StatusID = %i COMPLETED!!!\n\n", idx);fflush(stderr);
#endif

    return MFX_TASK_DONE;

} // mfxStatus D3D11VideoProcessor::QueryTaskStatus(mfxU32 idx)


mfxStatus D3D11VideoProcessor::QueryVariance(
    mfxU32 frameIndex,
    std::vector<UINT> &variance)
{
    variance.resize(m_varianceCaps.VarianceCount);
    if(!m_vpreCaps.bVariance || variance.empty() ) return MFX_ERR_NONE;

    memset((mfxU8*)&variance[0], 0, sizeof(m_varianceCaps.VarianceCount * m_varianceCaps.VarianceSize));

    //-----------------------------------------------------
    if( VPE_GUID_INTERFACE_V2 != m_iface.guid)
    {
        PREPROC_QUERY_VARIANCE_PARAMS queryVarianceParam = {0};

        queryVarianceParam.FrameNumber = frameIndex;// see D3D11VideoProcessor::Execute()
        queryVarianceParam.pVariances  = &variance[0];
        queryVarianceParam.VarianceBufferSize = m_varianceCaps.VarianceCount * m_varianceCaps.VarianceSize; //variance.size();

        PREPROC_MODE preprocMode;
        memset(&preprocMode, 0, sizeof(PREPROC_MODE));

        preprocMode.Version  = m_iface.version;
        preprocMode.Function = VPE_FN_QUERY_VARIANCE;
        preprocMode.pVarianceParams = &queryVarianceParam;

        HRESULT hRes = GetOutputExtension(
            &(m_iface.guid),
            sizeof(PREPROC_MODE),
            &preprocMode);
        CHECK_HRES(hRes);
    }
    else
    {
        VPE_VPREP_GET_VARIANCE_PARAMS queryVarianceParam = {0};

        queryVarianceParam.FrameCount = 1;//aya???
        queryVarianceParam.BufferSize = queryVarianceParam.FrameCount * ( sizeof(VPE_STATUS_PARAM) + ( m_varianceCaps.VarianceCount * m_varianceCaps.VarianceSize) );

        // aya: should be precalculated\pre-allocated on ::Init() stage
        std::vector<mfxU8> varianceBuffer(queryVarianceParam.BufferSize, 0);

        queryVarianceParam.pBuffer    = &varianceBuffer[0];

        VPE_FUNCTION iFunc = {0};
        iFunc.Function = VPE_FN_VPREP_GET_VARIANCE_PARAM;
        iFunc.pGetVarianceParam = &queryVarianceParam;

        UINT streamIndx = 0;
        HRESULT hRes = GetStreamExtension(
            streamIndx,
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
        CHECK_HRES(hRes);

        // aya: OK, try to find required variance
        mfxU8* pVarRepBuffer = (mfxU8*)&varianceBuffer[0];
        for(UINT frame = 0; frame < queryVarianceParam.FrameCount; frame++)
        {
            VPE_STATUS_PARAM stsParam = *((VPE_STATUS_PARAM*)pVarRepBuffer);
            if(VPE_STATUS_COMPLETED == stsParam.Status && frameIndex == stsParam.FrameId)
            {
                pVarRepBuffer += sizeof(VPE_STATUS_PARAM);
                for(size_t varIdx = 0; varIdx < variance.size(); varIdx++)
                {
                    variance[varIdx] = ((UINT*)pVarRepBuffer)[varIdx];
                }

                pVarRepBuffer += (m_varianceCaps.VarianceCount * m_varianceCaps.VarianceSize);
            }
        }
    }
    //-----------------------------------------------------

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoProcessor::QueryVariance(...)

mfxStatus D3D11VideoProcessor::GetEventHandle(HANDLE * pHandle)
{
    HRESULT hRes;
    VPE_FUNCTION iFunc = {0};

    MFX_CHECK_NULL_PTR1(pHandle);

    VPE_GET_BTL_EVENT_HANDLE_PARAM Param;
    Param.pEventHandle = pHandle;
    iFunc.Function   = VPE_FN_GET_BLT_EVENT_HANDLE;
    iFunc.pGetBltEventHandleParam = &Param;

    hRes = GetOutputExtension(
        &(m_iface.guid),
        sizeof(VPE_FUNCTION),
        &iFunc);
    CHECK_HRES(hRes);

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::CameraPipeActivate()
{
    HRESULT hRes;
    VPE_FUNCTION iFunc = {0};

    VPE_MODE_PARAM modeParam = {VPE_MODE_CAM_PIPE};
    iFunc.Function   = VPE_FN_MODE_PARAM;
    iFunc.pModeParam = &modeParam;

    hRes = SetOutputExtension(
        &(m_iface.guid),
        sizeof(VPE_FUNCTION),
        &iFunc);
    CHECK_HRES(hRes);

    // Disable status reporting since it breaks camera pipe.
    VPE_SET_STATUS_PARAM statusParam = {1, 0};
    iFunc.Function        = VPE_FN_SET_STATUS_PARAM;
    iFunc.pSetStatusParam = &statusParam;

    hRes = SetOutputExtension(
        &(m_iface.guid),
        sizeof(VPE_FUNCTION),
        &iFunc);
    CHECK_HRES(hRes);

    CAMPIPE_MODE          camMode;
    VPE_CP_ACTIVE_PARAMS  bActive;
    bActive.bActive = 1;

    memset((PVOID) &camMode, 0, sizeof(camMode));

    camMode.Function     = VPE_FN_CP_ACTIVATE_CAMERA_PIPE;
    camMode.pActive      = &bActive;

    hRes =  SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(camMode),
                    &camMode);
    if ( FAILED(hRes))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_bCameraMode = true;
    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::CameraPipeSetBlacklevelParams(CameraBlackLevelParams *params)
{
    HRESULT hRes;
    CAMPIPE_MODE          camMode;
    VPE_CP_BLACK_LEVEL_CORRECTION_PARAMS  blcParams;
    blcParams.bActive = 1;
    blcParams.B  = params->uB;
    blcParams.G0 = params->uG0;
    blcParams.G1 = params->uG1;
    blcParams.R  = params->uR;

    memset((PVOID) &camMode, 0, sizeof(camMode));

    camMode.Function     = VPE_FN_CP_BLACK_LEVEL_CORRECTION_PARAM;
    camMode.pBlackLevel  = &blcParams;
    hRes =  SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(camMode),
                    &camMode);
    if ( FAILED(hRes))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::CameraPipeSetLensParams(CameraLensCorrectionParams *params)
{
    HRESULT hRes;
    CAMPIPE_MODE          camMode;
    VPE_CP_LENS_GEOMENTRY_DISTORTION_CORRECTION  lensParams;
    lensParams.bActive = 1;
    for(int i = 0; i < 3; i++)
    {
        lensParams.a[i] = params->a[i];
        lensParams.b[i] = params->b[i];
        lensParams.c[i] = params->c[i];
        lensParams.d[i] = params->d[i];
    }
    memset((PVOID) &camMode, 0, sizeof(camMode));
    camMode.Function          = VPE_FN_LENS_GEOMETRY_DISTORTION_CORRECTION;
    camMode.pLensCorrect  = &lensParams;

    hRes =  SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(camMode),
                    &camMode);
    if ( FAILED(hRes))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::CameraPipeSet3DLUTParams(Camera3DLUTParams * params)
{

    HRESULT hRes;
    CAMPIPE_MODE          camMode;
    VPE_CP_LUT_PARAMS  lutParams;
    lutParams.bActive = 1;

    switch(params->LUTSize)
    {
    case(LUT33_SEG*LUT33_SEG*LUT33_SEG):
        memset(m_camera3DLUT33, 0, sizeof(m_camera3DLUT33));
        lutParams.LUTSize = LUT33_SEG;
        for(int i = 0; i < LUT33_SEG; i++)
        {
            for(int j = 0; j < LUT33_SEG; j++)
            {
                for(int z = 0; z < LUT33_SEG; z++)
                {
                    (*m_camera3DLUT33)[i][j][z].B = params->lut[(LUT33_SEG*LUT33_SEG*i+LUT33_SEG*j+z)].R;
                    (*m_camera3DLUT33)[i][j][z].G = params->lut[(LUT33_SEG*LUT33_SEG*i+LUT33_SEG*j+z)].G;
                    (*m_camera3DLUT33)[i][j][z].R = params->lut[(LUT33_SEG*LUT33_SEG*i+LUT33_SEG*j+z)].B;
                }
            }
        }
        lutParams.p33 = m_camera3DLUT33;
        break;

    case(LUT65_SEG*LUT65_SEG*LUT65_SEG):
        memset(m_camera3DLUT65, 0, sizeof(m_camera3DLUT65));
        lutParams.LUTSize = LUT65_SEG;
        for(int i = 0; i < LUT65_SEG; i++)
        {
            for(int j = 0; j < LUT65_SEG; j++)
            {
                for(int z = 0; z < LUT65_SEG; z++)
                {
                    (*m_camera3DLUT65)[i][j][z].B = params->lut[(LUT65_SEG*LUT65_SEG*i+LUT65_SEG*j+z)].R;
                    (*m_camera3DLUT65)[i][j][z].G = params->lut[(LUT65_SEG*LUT65_SEG*i+LUT65_SEG*j+z)].G;
                    (*m_camera3DLUT65)[i][j][z].R = params->lut[(LUT65_SEG*LUT65_SEG*i+LUT65_SEG*j+z)].B;
                }
            }
        }
        lutParams.p65 = m_camera3DLUT65;
        break;

    case(LUT17_SEG*LUT17_SEG*LUT17_SEG):
    default:

        memset(m_camera3DLUT17, 0, sizeof(m_camera3DLUT17));
        lutParams.LUTSize = LUT17_SEG;
        for(int i = 0; i < LUT17_SEG; i++)
        {
            for(int j = 0; j < LUT17_SEG; j++)
            {
                for(int z = 0; z < LUT17_SEG; z++)
                {
                    (*m_camera3DLUT17)[i][j][z].B = params->lut[(LUT17_SEG*LUT17_SEG*i+LUT17_SEG*j+z)].R;
                    (*m_camera3DLUT17)[i][j][z].G = params->lut[(LUT17_SEG*LUT17_SEG*i+LUT17_SEG*j+z)].G;
                    (*m_camera3DLUT17)[i][j][z].R = params->lut[(LUT17_SEG*LUT17_SEG*i+LUT17_SEG*j+z)].B;
                }
            }
        }
        lutParams.p17 = m_camera3DLUT17;
    break;
    }

    memset((PVOID) &camMode, 0, sizeof(camMode));
    camMode.Function   = VPE_FN_CP_3DLUT;
    camMode.p3DLUT     = &lutParams;

    hRes =  SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(camMode),
                    &camMode);

    if ( FAILED(hRes))
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}
mfxStatus D3D11VideoProcessor::CameraPipeSetCCMParams(CameraCCMParams *params)
{
    HRESULT hRes;
    CAMPIPE_MODE          camMode;
    VPE_CP_COLOR_CORRECTION_PARAMS  ccmParams;
    ccmParams.bActive = 1;
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            ccmParams.CCM[i][j] = params->CCM[i][j];
        }
    }

    memset((PVOID) &camMode, 0, sizeof(camMode));
    camMode.Function          = VPE_FN_CP_COLOR_CORRECTION_PARAM;
    camMode.pColorCorrection  = &ccmParams;

    hRes =  SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(camMode),
                    &camMode);
    if ( FAILED(hRes))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::CameraPipeSetForwardGammaParams(CameraForwardGammaCorrectionParams *params)
{
    HRESULT hRes;
    CAMPIPE_MODE          camMode;
    m_cameraFGC.bActive = 1;
    for(int i = 0; i < (int)m_cameraCaps.SegEntryForwardGamma; i++)
    {
        m_cameraFGC.pFGSegment[i].BlueChannelCorrectedValue  = params->Segment[i].BlueChannelCorrectedValue;
        m_cameraFGC.pFGSegment[i].GreenChannelCorrectedValue = params->Segment[i].GreenChannelCorrectedValue;
        m_cameraFGC.pFGSegment[i].RedChannelCorrectedValue   = params->Segment[i].RedChannelCorrectedValue;
        m_cameraFGC.pFGSegment[i].PixelValue                 = params->Segment[i].PixelValue;
    }

    memset((PVOID) &camMode, 0, sizeof(camMode));
    camMode.Function       = VPE_FN_CP_FORWARD_GAMMA_CORRECTION;
    camMode.pForwardGamma  = &m_cameraFGC;

    hRes =  SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(camMode),
                    &camMode);
    if ( FAILED(hRes))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::CameraPipeSetVignetteParams(CameraVignetteCorrectionParams *params)
{
    HRESULT hRes;
    CAMPIPE_MODE          camMode;
    VPE_CP_VIGNETTE_CORRECTION_PARAMS vignette = {0};
    vignette.bActive = 1;
    vignette.Height = params->Height;
    vignette.Width  = params->Width;
    vignette.Stride = params->Stride;

    // It's critical that app must allocate vignette correction map correctly in the memory.
    // Or maybe it's better to allocate it properly here? 
    vignette.pCorrectionMap = (VPE_CP_VIGNETTE_CORRECTION_ELEM *)params->pCorrectionMap;

    memset((PVOID) &camMode, 0, sizeof(camMode));
    camMode.Function   = VPE_FN_CP_VIGNETTE_CORRECTION_PARAM;
    camMode.pVignette  = &vignette;

    hRes =  SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(camMode),
                    &camMode);
    if ( FAILED(hRes))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::CameraPipeSetHotPixelParams(CameraHotPixelRemovalParams *params)
{
    HRESULT hRes;
    CAMPIPE_MODE          camMode;
    VPE_CP_HOT_PIXEL_PARAMS  hpParams;
    hpParams.bActive = 1;
    hpParams.PixelCountThreshold = params->uPixelCountThreshold;
    hpParams.PixelThresholdDifference = params->uPixelThresholdDifference;

    memset((PVOID) &camMode, 0, sizeof(camMode));
    camMode.Function   = VPE_FN_CP_HOT_PIXEL_PARAM;
    camMode.pHotPixel  = &hpParams;

    hRes =  SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(camMode),
                    &camMode);
    if ( FAILED(hRes))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::CameraPipeSetWhitebalanceParams(CameraWhiteBalanceParams *params)
{
    HRESULT hRes;
    CAMPIPE_MODE          camMode;
    VPE_CP_WHITE_BALANCE_PARAMS  wbParams;
    wbParams.bActive = 1;
    wbParams.Mode    = 1; // Force manual mode
    wbParams.BlueCorrection        = params->fB;
    wbParams.RedCorrection         = params->fR;
    wbParams.GreenBottomCorrection = params->fG0;
    wbParams.GreenTopCorrection    = params->fG1;

    memset((PVOID) &camMode, 0, sizeof(camMode));
    camMode.Function   = VPE_FN_CP_WHITE_BALANCE_PARAM;
    camMode.pWhiteBalance  = &wbParams;

    hRes =  SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(camMode),
                    &camMode);
    if ( FAILED(hRes))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::SetStreamScalingMode(UINT streamIndex, VPE_VPREP_SCALING_MODE_PARAM param)
{
    HRESULT hRes;
    VPE_FUNCTION iFunc;

    iFunc.Function          = VPE_FN_VPREP_SCALING_MODE_PARAM;
    iFunc.pScalingModeParam = &param;

    streamIndex = 0;
    hRes = SetStreamExtension(
            streamIndex,
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
    CHECK_HRES(hRes);

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoProcessor::ExecuteCameraPipe(mfxExecuteParams *pParams)
{
    MFX_CHECK_NULL_PTR1(pParams);

    mfxStatus    sts      = MFX_ERR_NONE;
    HRESULT      hRes     = S_OK;
    mfxFrameInfo pOutInfo = pParams->targetSurface.frameInfo;
    RECT         pRect    = {};

    if ( ! m_CameraSet )
    {
        m_bUseEventHandle = true;

        sts = CameraPipeActivate();
        MFX_CHECK_STS(sts);

        if ( pParams->bCamera3DLUT )
        {
            sts = CameraPipeSet3DLUTParams(&pParams->Camera3DLUT);
            MFX_CHECK_STS(sts);
        }
        if ( pParams->bCameraBlackLevelCorrection )
        {
            sts = CameraPipeSetBlacklevelParams(&pParams->CameraBlackLevel);
            MFX_CHECK_STS(sts);
        }

        if ( pParams->bCameraWhiteBalaceCorrection )
        {
            sts = CameraPipeSetWhitebalanceParams(&pParams->CameraWhiteBalance);
            MFX_CHECK_STS(sts);
        }

        if ( pParams->bCCM )
        {
            sts = CameraPipeSetCCMParams(&pParams->CCMParams);
            MFX_CHECK_STS(sts);
        }

        if ( pParams->bCameraLensCorrection )
        {
            sts = CameraPipeSetLensParams(&pParams->CameraLensCorrection);
            MFX_CHECK_STS(sts);
        }
        if ( pParams->bCameraHotPixelRemoval )
        {
            sts = CameraPipeSetHotPixelParams(&pParams->CameraHotPixel);
            MFX_CHECK_STS(sts);
        }

        if ( pParams->bCameraGammaCorrection )
        {
            sts = CameraPipeSetForwardGammaParams(&pParams->CameraForwardGammaCorrection);
            MFX_CHECK_STS(sts);
        }

        if ( pParams->bCameraVignetteCorrection )
        {
            sts = CameraPipeSetVignetteParams(&pParams->CameraVignetteCorrection);
            MFX_CHECK_STS(sts);
        }
    }

    pRect.top = pRect.left = 0;
    pRect.bottom = pOutInfo.Height;
    pRect.right  = pOutInfo.Width;
    SetOutputTargetRect(TRUE, &pRect);

    mfxHDL inputSurface;
    inputSurface = pParams->pRefSurfaces[0].hdl.first;
    MFX_CHECK_NULL_PTR1(inputSurface);

    // Get video processor stream. Camera works with a single stream at the moment
    std::map<void *, D3D11_VIDEO_PROCESSOR_STREAM *>::iterator it_stream;
    D3D11_VIDEO_PROCESSOR_STREAM *videoProcessorStream;
    it_stream = m_videoProcessorStreams.find(inputSurface);
    if (m_videoProcessorStreams.end() == it_stream)
    {
        // Alocate new Video processor stream
        videoProcessorStream = new D3D11_VIDEO_PROCESSOR_STREAM;
        memset(videoProcessorStream, 0, sizeof(D3D11_VIDEO_PROCESSOR_STREAM));
        m_videoProcessorStreams.insert(std::pair<void *, D3D11_VIDEO_PROCESSOR_STREAM *>(inputSurface, videoProcessorStream));
        videoProcessorStream->Enable = TRUE;
        videoProcessorStream->OutputIndex = PtrToUlong(pParams->targetSurface.hdl.second);
        videoProcessorStream->InputFrameOrField = 0;
        SetStreamFrameFormat(0, D3D11PictureStructureMapping(pParams->pRefSurfaces[0].frameInfo.PicStruct));

        SetStreamSourceRect(0, TRUE, &pRect);
        SetStreamDestRect(0, TRUE, &pRect);

        D3D11_VIDEO_PROCESSOR_COLOR_SPACE inColorSpace;
        inColorSpace.Usage = 0;
        inColorSpace.RGB_Range = 1;
        inColorSpace.YCbCr_Matrix = 0;
        inColorSpace.YCbCr_xvYCC = 0;
        inColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_UNDEFINED;
        SetStreamColorSpace(0, &inColorSpace);

        D3D11_VIDEO_PROCESSOR_COLOR_SPACE outColorSpace;
        outColorSpace.Usage = 0;
        outColorSpace.RGB_Range = 1;
        outColorSpace.YCbCr_Matrix = 0;
        outColorSpace.YCbCr_xvYCC = 0;
        outColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_UNDEFINED;
        SetOutputColorSpace(&outColorSpace);
    }
    else
    {
        // re-use previously allocated
        videoProcessorStream = it_stream->second;
    }

    

    // Get Video process input view 
    std::map<void *, ID3D11VideoProcessorInputView *>::iterator it_inputView;
    it_inputView = m_videoProcessorInputViews.find(inputSurface);
    ID3D11VideoProcessorInputView* pInputView = 0;

    if ( it_inputView == m_videoProcessorInputViews.end() )
    {
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc;
        inputDesc.ViewDimension        = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inputDesc.Texture2D.ArraySlice = PtrToUlong(pParams->pRefSurfaces[0].hdl.second);
        inputDesc.FourCC               = BayerFourCC2FourCC(pParams->pRefSurfaces[0].frameInfo.FourCC);
        inputDesc.Texture2D.MipSlice   = 0;
        ID3D11Resource* pInputResource = (ID3D11Resource *) (ID3D11Texture2D *)inputSurface;
    
        hRes = m_pVideoDevice->CreateVideoProcessorInputView(
                pInputResource,
                m_pVideoProcessorEnum,
                &inputDesc,
                &pInputView);
        CHECK_HRES(hRes);
        m_videoProcessorInputViews.insert(std::pair<void *, ID3D11VideoProcessorInputView *>(inputSurface, pInputView));
    }
    else
    {
        pInputView = it_inputView->second;
    }

    videoProcessorStream->ppPastSurfaces   = 0;
    videoProcessorStream->PastFrames       = pParams->bkwdRefCount;
    videoProcessorStream->pInputSurface    = pInputView;
    videoProcessorStream->ppFutureSurfaces = 0;
    videoProcessorStream->FutureFrames     = pParams->fwdRefCount;

    // Get Video process input view 
    mfxHDL outputSurface = pParams->targetSurface.hdl.first;
    std::map<void *, ID3D11VideoProcessorOutputView *>::iterator it_outputView;
    it_outputView = m_videoProcessorOutputViews.find(outputSurface);
    ID3D11VideoProcessorOutputView * outputView; 
    if ( it_outputView == m_videoProcessorOutputViews.end() )
    {
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc;
        memset((void*)&outputDesc, 0, sizeof(outputDesc));
        outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outputDesc.Texture2DArray.ArraySize = 0;
        outputDesc.Texture2DArray.MipSlice = 0;
        outputDesc.Texture2DArray.FirstArraySlice = 0;

        hRes = m_pVideoDevice->CreateVideoProcessorOutputView(
            (ID3D11Resource *) pParams->targetSurface.hdl.first,
            m_pVideoProcessorEnum,
            &outputDesc,
            &outputView);
        CHECK_HRES(hRes);
        m_videoProcessorOutputViews.insert(std::pair<void *, ID3D11VideoProcessorOutputView *>(outputSurface, outputView));
    }
    else
    {
        outputView = it_outputView->second;
    }

    hRes = VideoProcessorBlt(
            outputView,
            pParams->statusReportID,
            1,
            videoProcessorStream);
    CHECK_HRES(hRes);

    m_CameraSet = true;
    return sts;
}

mfxStatus D3D11VideoProcessor::Execute(mfxExecuteParams *pParams)
{
#ifdef DEBUG_DETAIL_INFO
    printf("\n\n---------- \n Submit Task::StatusID = %i \n----------\n\n", pParams->statusReportID);fflush(stderr);
#endif
    mfxStatus sts;
    MFX_CHECK_NULL_PTR1(pParams);
    if ( pParams->bCameraPipeEnabled )
    {
        // Camera pipe has its own execution flow that is optimized for perf and is 
        // not 100% compatible with other types of VPP execution.
        return ExecuteCameraPipe(pParams);
    }

    mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);

    // [1] target rectangle
    RECT pRect;
    pRect.top  = 0;
    pRect.left = 0;
    pRect.bottom = outInfo->Height;
    pRect.right  = outInfo->Width;
    SetOutputTargetRect(TRUE, &pRect);

    // [2] destination cropping
    pRect.top  = outInfo->CropY;
    pRect.left = outInfo->CropX;
    pRect.bottom = outInfo->CropH;
    pRect.right  = outInfo->CropW;
    pRect.bottom += outInfo->CropY;
    pRect.right  += outInfo->CropX;
    SetStreamDestRect(0, TRUE, &pRect);

    if (m_vpreCaps.bScalingMode)
    {
        VPE_VPREP_SCALING_MODE_PARAM param = {0};
        switch(pParams->scalingMode)
        {
        case MFX_SCALING_MODE_LOWPOWER:
            param.FastMode = false;
            break;

        case MFX_SCALING_MODE_QUALITY:
        case MFX_SCALING_MODE_DEFAULT:
        default:
            param.FastMode = true;
            break;
        }
        sts = SetStreamScalingMode(0, param);
        MFX_CHECK_STS(sts);
    }

    // [4] ProcAmp
    if(pParams->bEnableProcAmp)
    {
        SetStreamFilter(
            0,
            D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS,
            TRUE,
            (int)(pParams->Brightness * m_multiplier.brightness));

        SetStreamFilter(
            0,
            D3D11_VIDEO_PROCESSOR_FILTER_HUE,
            TRUE,
            (int)(pParams->Hue * m_multiplier.hue));

        SetStreamFilter(
            0,
            D3D11_VIDEO_PROCESSOR_FILTER_SATURATION,
            TRUE,
            (int)(pParams->Saturation*m_multiplier.saturation));

        SetStreamFilter(
            0,
            D3D11_VIDEO_PROCESSOR_FILTER_CONTRAST,
            TRUE,
            (int)(pParams->Contrast*m_multiplier.contrast));
    }
    if(pParams->rotation)
    {
        int angle;
        switch(pParams->rotation){
            case MFX_ANGLE_90:
                angle = (int)D3D11_VIDEO_PROCESSOR_ROTATION_90;break;
            case MFX_ANGLE_180:
                angle = (int)D3D11_VIDEO_PROCESSOR_ROTATION_180;break;
            case MFX_ANGLE_270:
                angle = (int)D3D11_VIDEO_PROCESSOR_ROTATION_270;break;
            default:
                angle = 0;
        }
        SetStreamRotation(
            0,
            TRUE,
            angle);
    }

    // [5] Detail
    if (pParams->detailFactor > 0)
    {
        SetStreamFilter(0, D3D11_VIDEO_PROCESSOR_FILTER_EDGE_ENHANCEMENT, TRUE, pParams->detailFactor);
    }

    // [6] Denoise
    if (pParams->denoiseFactor > 0 || pParams->bDenoiseAutoAdjust)
    {
        const mfxU16 DEFAULT_NOISE_LEVEL_D3D11 = 0; // 0 is signal to driver use Auto Mode
        mfxU16 noiseLevel = (pParams->bDenoiseAutoAdjust) ? DEFAULT_NOISE_LEVEL_D3D11 : pParams->denoiseFactor;
        SetStreamFilter(0, D3D11_VIDEO_PROCESSOR_FILTER_NOISE_REDUCTION, TRUE, noiseLevel);
    }

    HRESULT hRes;
    if(VPE_GUID_INTERFACE_V2 != m_iface.guid)
    {
        // [7] Set VPE Preproc Params
        SET_PREPROC_PARAMS setParams;
        setParams.bEnablePreProcMode     = true;
        setParams.bSceneDetectionEnable  = 0;
        setParams.iTargetInterlacingMode = pParams->iTargetInterlacingMode;
        setParams.bTargetInSysMemory     = 0;
        setParams.bVarianceQuery         = pParams->bVarianceEnable;
        setParams.bFieldWeaving          = pParams->bFieldWeaving;

        //take into consederation. if you change it, QueryVariance should be changed too
        setParams.StatusReportID         = pParams->statusReportID;// + 1;

        // [7.1] mode
        PREPROC_MODE preprocMode;
        memset(&preprocMode, 0, sizeof(PREPROC_MODE));

        preprocMode.Version        = m_iface.version;
        preprocMode.Function       = VPE_FN_SET_PREPROC_MODE;
        preprocMode.pPreprocParams = &setParams;

        hRes = SetOutputExtension(
            &(m_iface.guid),
            sizeof(PREPROC_MODE),
            &preprocMode);
        CHECK_HRES(hRes);

        // [7.2] Set IS Param
        SET_IS_PARAMS istabParams;
        istabParams.IStabMode = VPREP_ISTAB_MODE_NONE;
        if(pParams->bImgStabilizationEnable)
        {
            istabParams.IStabMode = (VPREP_ISTAB_MODE)pParams->istabMode;
        }

        //preprocMode.Version = m_iface.version;
        preprocMode.Function  = VPE_FN_SET_IS_PARAMS;
        preprocMode.pISParams = &istabParams;

        hRes = SetOutputExtension(
            &(m_iface.guid),
            sizeof(PREPROC_MODE),
            &preprocMode);
        CHECK_HRES(hRes);
    }
    else
    {
        // [7.2] Set IS Param
        VPE_VPREP_ISTAB_PARAM istabParam = {VPE_VPREP_ISTAB_MODE_NONE};
        if(pParams->bImgStabilizationEnable)
        {
            istabParam.Mode = (VPE_VPREP_ISTAB_MODE)pParams->istabMode;
        }

        VPE_FUNCTION iFunc;
        iFunc.Function    = VPE_FN_VPREP_ISTAB_PARAM;
        iFunc.pIStabParam = &istabParam;

        UINT streamIndex = 0;
        hRes = SetStreamExtension(
            streamIndex,
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
        CHECK_HRES(hRes);

        // [7.3] Set INTERLACE PARAM
        VPE_VPREP_INTERLACE_PARAM interlaceParam = {VPE_VPREP_INTERLACE_MODE_NONE};
        if(pParams->bFieldWeaving)
        {
            interlaceParam.Mode = VPE_VPREP_INTERLACE_MODE_FIELD_WEAVE;
        }
        else if(pParams->iTargetInterlacingMode)
        {
            interlaceParam.Mode = VPE_VPREP_INTERLACE_MODE_ISCALE;
        }

        iFunc.Function       = VPE_FN_VPREP_INTERLACE_PARAM;
        iFunc.pInterlacePram = &interlaceParam;

        hRes = SetStreamExtension(
            streamIndex,
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
        CHECK_HRES(hRes);


        // [6.1.5] Set Variance Param
        VPE_VPREP_SET_VARIANCE_PARAM  varianceSetParam = {VPREP_VARIANCE_TYPE_NONE};

        if(pParams->bVarianceEnable)
        {
            varianceSetParam.Type = VPREP_VARIANCE_TYPE_1;
        }

        iFunc.Function          = VPE_FN_VPREP_SET_VARIANCE_PARAM;
        iFunc.pSetVarianceParam = &varianceSetParam;

        hRes = SetStreamExtension(
            streamIndex,
            &(m_iface.guid),
            sizeof(VPE_FUNCTION),
            &iFunc);
        CHECK_HRES(hRes);

    }

    // [9] surface registration
    D3D11_VIDEO_PROCESSOR_STREAM *videoProcessorStreams = new D3D11_VIDEO_PROCESSOR_STREAM[pParams->refCount];
    MFX_CHECK_NULL_PTR1(videoProcessorStreams);

    memset(videoProcessorStreams, 0, sizeof(D3D11_VIDEO_PROCESSOR_STREAM) * pParams->refCount);

    int refIdx = 0;

    for (refIdx = 0; refIdx < pParams->refCount; refIdx++)
    {
        videoProcessorStreams[refIdx].Enable = TRUE;
        videoProcessorStreams[refIdx].OutputIndex = 0;//PtrToUlong(pParams->targetSurface.hdl.second);
        videoProcessorStreams[refIdx].InputFrameOrField = pParams->statusReportID;
    }

    // [10] advanced configuration
    // FRC
    for (refIdx = 0; refIdx < pParams->refCount; refIdx++)
    {
        if( pParams->bFRCEnable )
        {
            if(false == m_frcState.m_isInited)
            {
                m_frcState.m_inputFramesOrFieldsPerCycle = pParams->customRateData.inputFramesOrFieldPerCycle;
                m_frcState.m_outputIndexCountPerCycle    = pParams->customRateData.outputIndexCountPerCycle;
                m_frcState.m_isInited = true;

                DXGI_RATIONAL customRational = {
                pParams->customRateData.customRate.FrameRateExtN,
                pParams->customRateData.customRate.FrameRateExtD};

                SetStreamOutputRate(refIdx, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_CUSTOM, false, &customRational);
            }

            videoProcessorStreams[refIdx].InputFrameOrField  = m_frcState.m_inputIndx;
            videoProcessorStreams[refIdx].OutputIndex        = m_frcState.m_outputIndx;

            // update
            m_frcState.m_outputIndx++;
            if(m_frcState.m_outputIndexCountPerCycle == m_frcState.m_outputIndx)
            {
                m_frcState.m_outputIndx = 0;
                m_frcState.m_inputIndx += m_frcState.m_inputFramesOrFieldsPerCycle;
            }
        }
        // mode 30i->60p: if input startPts != targetPts it mean second output fom DI required
        else if(pParams->iDeinterlacingAlgorithm)
        {
            if(pParams->pRefSurfaces[pParams->bkwdRefCount].startTimeStamp != pParams->targetTimeStamp)
            {
                videoProcessorStreams[refIdx].OutputIndex  = 1;
            }
        }
    }

    // [11] reference samples
    m_pInputView.resize(pParams->refCount);

    for (refIdx = 0; refIdx < pParams->refCount; refIdx++)
    {
        SetStreamFrameFormat(refIdx, D3D11PictureStructureMapping(pParams->pRefSurfaces[refIdx].frameInfo.PicStruct));

        if ( pParams->pRefSurfaces[refIdx].frameInfo.FourCC == MFX_FOURCC_A2RGB10    ||
             pParams->pRefSurfaces[refIdx].frameInfo.FourCC == MFX_FOURCC_ARGB16     ||
             pParams->pRefSurfaces[refIdx].frameInfo.FourCC == MFX_FOURCC_AYUV       ||
             pParams->pRefSurfaces[refIdx].frameInfo.FourCC == MFX_FOURCC_AYUV_RGB4  ||
             pParams->pRefSurfaces[refIdx].frameInfo.FourCC == MFX_FOURCC_RGB4)
        {
            // WA: if app explicitly requested LOW_POWER (e.g. using SFC instead of AVS), AlphaFillMode should not be used
            // because driver will fallback silently to AVS.
            if ( ! m_vpreCaps.bScalingMode || pParams->scalingMode != MFX_SCALING_MODE_LOWPOWER )
            {
                // Preserve alpha channel. If this call is not done, alpha channel of the output surface will be
                // filed with 0xFF.
                SetOutputAlphaFillMode(D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_SOURCE_STREAM, refIdx);
            }
        }

        mfxDrvSurface* pInputSample = &(pParams->pRefSurfaces[refIdx]);

        // source cropping
        mfxFrameInfo *inInfo = &(pInputSample->frameInfo);
        pRect.top    = inInfo->CropY;
        pRect.left   = inInfo->CropX;
        pRect.bottom = inInfo->CropH;
        pRect.right  = inInfo->CropW;
        pRect.bottom += inInfo->CropY;
        pRect.right  += inInfo->CropX;

        SetStreamSourceRect(refIdx, TRUE, &pRect);

        // destination cropping
        if (pParams->bComposite)
        {
            // for sub-streams use DstRect info from ext buffer set by app
            MFX_CHECK(refIdx < (int) pParams->dstRects.size(), MFX_ERR_UNKNOWN);
            const DstRect& rec = pParams->dstRects[refIdx];
            pRect.top = rec.DstY;
            pRect.left = rec.DstX;
            pRect.bottom = rec.DstY + rec.DstH;
            pRect.right  = rec.DstX + rec.DstW;

            FLOAT GlobalAlpha = rec.GlobalAlphaEnable ? rec.GlobalAlpha / 255.0f : 1.0f;
            SetStreamAlpha(refIdx, TRUE, GlobalAlpha);
            SetStreamLumaKey(refIdx, rec.LumaKeyEnable, rec.LumaKeyMin / 255.0f, rec.LumaKeyMax / 255.0f);
        }
        else
        {
            pRect.top = outInfo->CropY;
            pRect.left = outInfo->CropX;
            pRect.bottom = outInfo->CropH + outInfo->CropY;
            pRect.right  = outInfo->CropW + outInfo->CropX;
        }

        SetStreamDestRect(refIdx, TRUE, &pRect);

        mfxU32 fourCC = 0;

        if( inInfo->FourCC == MFX_FOURCC_IMC3 ||
            inInfo->FourCC == MFX_FOURCC_YUV400 ||
            inInfo->FourCC == MFX_FOURCC_YUV411 ||
            inInfo->FourCC == MFX_FOURCC_YUV422H ||
            inInfo->FourCC == MFX_FOURCC_YUV422V ||
            inInfo->FourCC == MFX_FOURCC_YUV444 ||
            inInfo->FourCC == MFX_FOURCC_R16 ||
            inInfo->FourCC == MFX_FOURCC_RGBP)
        {
            D3D11_VIDEO_PROCESSOR_COLOR_SPACE inColorSpace;
            inColorSpace.Usage = 0;
            inColorSpace.RGB_Range = 1;
            inColorSpace.YCbCr_Matrix = 0;
            inColorSpace.YCbCr_xvYCC = 0;
            inColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_UNDEFINED;

            SetStreamColorSpace(refIdx, &inColorSpace);

            D3D11_VIDEO_PROCESSOR_COLOR_SPACE outColorSpace;
            outColorSpace.Usage = 0;
            outColorSpace.RGB_Range = 1;
            outColorSpace.YCbCr_Matrix = 0;
            outColorSpace.YCbCr_xvYCC = 0;
            outColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_UNDEFINED;

            SetOutputColorSpace(&outColorSpace);

            fourCC = inInfo->FourCC;

            if ( IsBayerFormat(fourCC) )
            {
                fourCC = BayerFourCC2FourCC(inInfo->FourCC);;
            }

            /* [6.1.7] Set YUV Range Param
            // Full range is implemented only in 15.36 driver in DX11 and has a brightness problem
            // Therefore don't set flag

            VPE_VPREP_YUV_RANGE_PARAM yuvRangeParam;
            yuvRangeParam.bFullRangeEnabled = 1;
            if(VPE_GUID_INTERFACE_V2 == m_iface.guid){
                VPE_FUNCTION iFunc;
                iFunc.Function = VPE_FN_VPREP_YUV_RANGE_PARAM;
                iFunc.pYUVRangeParam = &yuvRangeParam;

                UINT streamIndex = 0;

                hRes = SetStreamExtension(
                    streamIndex,
                    &(m_iface.guid),
                    sizeof(VPE_FUNCTION),
                    &iFunc);
                CHECK_HRES(hRes);

                hRes = SetOutputExtension(
                    &(m_iface.guid),
                    sizeof(VPE_FUNCTION),
                    &iFunc);
                CHECK_HRES(hRes);
            }
            */
        }
        else
        {
            D3D11_VIDEO_PROCESSOR_COLOR_SPACE inColorSpace;
            inColorSpace.Usage = 0;
            inColorSpace.RGB_Range       = 0; 
            inColorSpace.YCbCr_Matrix    = 0; // bt601
            inColorSpace.YCbCr_xvYCC     = 0;
            inColorSpace.Nominal_Range   = (inInfo->FourCC == MFX_FOURCC_RGB4) ? D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255 : D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;

            SetStreamColorSpace(refIdx, &inColorSpace);

            D3D11_VIDEO_PROCESSOR_COLOR_SPACE outColorSpace;
            outColorSpace.Usage         = 0;
            outColorSpace.RGB_Range     = 0;//;
            outColorSpace.YCbCr_Matrix  = 0; // bt601
            outColorSpace.YCbCr_xvYCC   = 0;
            outColorSpace.Nominal_Range = (outInfo->FourCC == MFX_FOURCC_RGB4) ? D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255 : D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;

            SetOutputColorSpace(&outColorSpace);
        }

        /* Video signal info */
        size_t index = static_cast<size_t>(refIdx) < pParams->VideoSignalInfo.size() ? refIdx : (pParams->VideoSignalInfo.size()-1);
        if (pParams->VideoSignalInfo[index].enabled)
        {
            D3D11_VIDEO_PROCESSOR_COLOR_SPACE inColorSpace;
            inColorSpace.Usage         = 0;
            inColorSpace.RGB_Range     = 0; // 16-245 range
            inColorSpace.YCbCr_Matrix  = 0; // bt601
            inColorSpace.YCbCr_xvYCC   = 0;
            inColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;

            if (pParams->VideoSignalInfo[index].NominalRange == D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255)
            {
                inColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
                inColorSpace.RGB_Range     = 1;
            }
            if (pParams->VideoSignalInfo[index].TransferMatrix == MFX_TRANSFERMATRIX_BT709)
            {
                inColorSpace.YCbCr_Matrix  = 1;
            }

            SetStreamColorSpace(refIdx, &inColorSpace);
        }

        if(pParams->VideoSignalInfoOut.enabled)
        {
            D3D11_VIDEO_PROCESSOR_COLOR_SPACE outColorSpace;
            outColorSpace.Usage         = 0;
            outColorSpace.RGB_Range     = 0;
            outColorSpace.YCbCr_Matrix  = 0;
            outColorSpace.YCbCr_xvYCC   = 0;
            outColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;

            if (pParams->VideoSignalInfoOut.NominalRange == D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255)
            {
                outColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
                outColorSpace.RGB_Range     = 1;
            }

            if (pParams->VideoSignalInfoOut.TransferMatrix == MFX_TRANSFERMATRIX_BT709)
            {
                outColorSpace.YCbCr_Matrix = 1;
            }

            SetOutputColorSpace(&outColorSpace);
        }

        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc;
        inputDesc.ViewDimension        = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inputDesc.Texture2D.ArraySlice = PtrToUlong(pParams->pRefSurfaces[refIdx].hdl.second);
        inputDesc.FourCC               = fourCC;
        inputDesc.Texture2D.MipSlice   = 0;

        ID3D11Resource*   pInputResource            = (ID3D11Resource *) (ID3D11Texture2D *)pParams->pRefSurfaces[refIdx].hdl.first;
        ID3D11VideoProcessorInputView** ppInputView = &(m_pInputView[refIdx]);

        hRes = m_pVideoDevice->CreateVideoProcessorInputView(
            pInputResource,
            m_pVideoProcessorEnum,
            &inputDesc,
            ppInputView);
        CHECK_HRES(hRes);

        if(m_file)
        {
             mfxFrameData data = { 0 };
             data.MemId = pParams->pRefSurfaces[refIdx].memId;
             bool external = pParams->pRefSurfaces[refIdx].bExternal;
             mfxFrameInfo info = pParams->pRefSurfaces[refIdx].frameInfo;

            MfxVppDump::WriteFrameData(
                m_file,
                m_core,
                data,
                info,
                external);
        }
    }

    // [12] background color
    BOOL bYCbCr = TRUE;
    D3D11_VIDEO_COLOR Color = {0};

    if (outInfo->FourCC == MFX_FOURCC_NV12 ||
        outInfo->FourCC == MFX_FOURCC_YV12 ||
        outInfo->FourCC == MFX_FOURCC_NV16 ||
        outInfo->FourCC == MFX_FOURCC_YUY2 ||
        outInfo->FourCC == MFX_FOURCC_P010 ||
        outInfo->FourCC == MFX_FOURCC_P210)
    {
        bYCbCr = TRUE;

        Color.YCbCr.A  = ((pParams->iBackgroundColor >> 24) & 0xff) / 255.0f;
        Color.YCbCr.Y  = ((pParams->iBackgroundColor >> 16) & 0xff) / 255.0f;
        Color.YCbCr.Cb = ((pParams->iBackgroundColor >>  8) & 0xff) / 255.0f;
        Color.YCbCr.Cr = ((pParams->iBackgroundColor      ) & 0xff) / 255.0f;
    }
    if (outInfo->FourCC == MFX_FOURCC_RGB3    ||
        outInfo->FourCC == MFX_FOURCC_RGB4    ||
        outInfo->FourCC == MFX_FOURCC_BGR4    ||
        outInfo->FourCC == MFX_FOURCC_A2RGB10 ||
        outInfo->FourCC == MFX_FOURCC_ARGB16  ||
        outInfo->FourCC == MFX_FOURCC_R16)
    {
        bYCbCr = FALSE;

        Color.RGBA.A = ((pParams->iBackgroundColor >> 24) & 0xff) / 255.0f;
        Color.RGBA.R = ((pParams->iBackgroundColor >> 16) & 0xff) / 255.0f;
        Color.RGBA.G = ((pParams->iBackgroundColor >>  8) & 0xff) / 255.0f;
        Color.RGBA.B = ((pParams->iBackgroundColor      ) & 0xff) / 255.0f;
    }

    SetOutputBackgroundColor(bYCbCr, &Color);

    UINT StreamCount;

    if (pParams->bComposite)
    {
        StreamCount = pParams->refCount;

        for (refIdx = 0; refIdx < pParams->refCount; refIdx++)
        {
            videoProcessorStreams[refIdx].ppPastSurfaces = NULL;
            videoProcessorStreams[refIdx].PastFrames     = NULL;

            videoProcessorStreams[refIdx].pInputSurface  = m_pInputView[refIdx];

            videoProcessorStreams[refIdx].ppFutureSurfaces = NULL;
            videoProcessorStreams[refIdx].FutureFrames     = NULL;
        }
    }
    else
    {
        StreamCount = 1;

        videoProcessorStreams[0].ppPastSurfaces = (pParams->bkwdRefCount > 0) ? &m_pInputView[0] : NULL;
        videoProcessorStreams[0].PastFrames     = pParams->bkwdRefCount;

        videoProcessorStreams[0].pInputSurface  = m_pInputView[pParams->bkwdRefCount];

        videoProcessorStreams[0].ppFutureSurfaces = (pParams->fwdRefCount > 0) ? &(m_pInputView[pParams->bkwdRefCount + 1]) : NULL;
        videoProcessorStreams[0].FutureFrames     = pParams->fwdRefCount;
    }

    sts = ExecuteBlt(
        (ID3D11Texture2D *)pParams->targetSurface.hdl.first,
        PtrToUlong(pParams->targetSurface.hdl.second),
        StreamCount,
        videoProcessorStreams,
        pParams->statusReportID);

    SAFE_DELETE_ARRAY(videoProcessorStreams);

    for( refIdx = 0; refIdx < pParams->refCount; refIdx++ )
    {
        SAFE_RELEASE(m_pInputView[refIdx]);
    }
    SAFE_RELEASE(m_pOutputView);

    return sts;

} // mfxStatus D3D11VideoProcessor::Execute(mfxExecuteParams *pParams)

mfxStatus D3D11VideoProcessor::ExecuteBlt(
    ID3D11Texture2D *pOutputSurface,
    mfxU32 outIndex,
    UINT StreamCount,
    D3D11_VIDEO_PROCESSOR_STREAM *pStreams,
    mfxU32 statusReportID)
{
    statusReportID;

    HRESULT hRes = S_OK;

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc;
    memset((void*)&outputDesc, 0, sizeof(outputDesc));
    outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputDesc.Texture2DArray.ArraySize = 1;
    outputDesc.Texture2DArray.MipSlice = 0;
    outputDesc.Texture2DArray.FirstArraySlice = outIndex;

    m_pOutputResource = (ID3D11Resource *) pOutputSurface;

    if (NULL != m_pOutputView)
    {
        SAFE_RELEASE(m_pOutputView);
    }

    hRes = m_pVideoDevice->CreateVideoProcessorOutputView(
        m_pOutputResource,
        m_pVideoProcessorEnum,
        &outputDesc,
        &m_pOutputView);
    CHECK_HRES(hRes);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "m_pVideoContext->VideoProcessorBlt()");
        hRes = VideoProcessorBlt(
            m_pOutputView,
            statusReportID,
            StreamCount,
            pStreams);
    }

    CHECK_HRES(hRes);

    return MFX_ERR_NONE;

} // void D3D11VideoProcessor::Execute(ID3D11VideoProcessorOutputView *pView, UINT OutputFrame, UINT StreamCount, const D3D11_VIDEO_PROCESSOR_STREAM *pStreams)


HRESULT D3D11VideoProcessor::VideoProcessorBlt(
    ID3D11VideoProcessorOutputView *pView,
    UINT OutputFrame,
    UINT StreamCount,
    const D3D11_VIDEO_PROCESSOR_STREAM *pStreams)
{
    //-----------------------------------------------------
#ifdef DEBUG_DETAIL_INFO
    printf("\n----------\n");fflush(stderr);
    printf("State BEFORE  VideoProcessorBlt()  call\n");fflush(stderr);
    printf("pVideoProcessor = %p\n", m_pVideoProcessor);fflush(stderr);
    printf("pView           = %p\n", pView);fflush(stderr);
    printf("OutputFrame     = %p\n", OutputFrame);fflush(stderr);
    printf("StreamCount     = %p\n", StreamCount);fflush(stderr);
    for (UINT idx = 0; idx < StreamCount; idx++)
    {
        printf("  pStreams[%i]    = %p\n", idx, &pStreams[idx]);fflush(stderr);
        printf("    Enable             = %i\n", pStreams[idx].Enable);fflush(stderr);
        printf("    OutputIndex        = %i\n", pStreams[idx].OutputIndex);fflush(stderr);
        printf("    InputFrameOrField  = %i\n", pStreams[idx].InputFrameOrField);fflush(stderr);
        printf("    PastFrames         = %i\n", pStreams[idx].PastFrames);fflush(stderr);
        printf("    ppPastSurfaces     = %p\n", pStreams[idx].ppPastSurfaces);fflush(stderr);

        for( int idxPast = 0; idxPast < (int)pStreams->PastFrames; idxPast++ )
        {
            printf("        ppPastSurfaces[%i]     = %p\n", idxPast, pStreams[idx].ppPastSurfaces[idxPast]);fflush(stderr);
        }

        printf("    FutureFrames       = %i\n", pStreams[idx].FutureFrames);fflush(stderr);
        printf("    ppFutureSurfaces   = %p\n", pStreams[idx].ppFutureSurfaces);fflush(stderr);
        for( int idxNext = 0; idxNext < (int)pStreams->FutureFrames; idxNext++ )
        {
            printf("        ppFutureSurfaces[%i]     = %p\n", idxNext, pStreams[idx].ppFutureSurfaces[idxNext]);fflush(stderr);
        }

        printf("    pInputSurface      = %p\n", pStreams[idx].pInputSurface);fflush(stderr);

        printf("    ppPastSurfacesRight= %p\n", pStreams[idx].ppPastSurfacesRight);fflush(stderr);
        printf("    pInputSurfaceRight = %p\n", pStreams[idx].pInputSurfaceRight);fflush(stderr);
        printf("    ppFutureSurfacesRight   = %p\n", pStreams[idx].ppFutureSurfacesRight);fflush(stderr);
    }
#endif
    //-----------------------------------------------------

    HRESULT hRes = m_pVideoContext->VideoProcessorBlt(
            m_pVideoProcessor,
            pView,
            OutputFrame,
            StreamCount,
            pStreams);


    //-----------------------------------------------------
#ifdef DEBUG_DETAIL_INFO

    printf("\n sts = %i \n", hRes);fflush(stderr);

    printf("\n----------\n");fflush(stderr);
    printf("State AFTER  VideoProcessorBlt()  call\n");fflush(stderr);
    printf("pVideoProcessor = %p\n", m_pVideoProcessor);fflush(stderr);
    printf("pView           = %p\n", pView);fflush(stderr);
    printf("OutputFrame     = %p\n", OutputFrame);fflush(stderr);
    printf("StreamCount     = %p\n", StreamCount);fflush(stderr);
    for (UINT idx = 0; idx < StreamCount; idx++)
    {
        printf("  pStreams[%i]    = %p\n", idx, &pStreams[idx]);fflush(stderr);
        printf("    Enable             = %i\n", pStreams[idx].Enable);fflush(stderr);
        printf("    OutputIndex        = %i\n", pStreams[idx].OutputIndex);fflush(stderr);
        printf("    InputFrameOrField  = %i\n", pStreams[idx].InputFrameOrField);fflush(stderr);
        printf("    PastFrames         = %i\n", pStreams[idx].PastFrames);fflush(stderr);
        printf("    ppPastSurfaces     = %p\n", pStreams[idx].ppPastSurfaces);fflush(stderr);

        for( int idxPast = 0; idxPast < (int)pStreams->PastFrames; idxPast++ )
        {
            printf("        ppPastSurfaces[%i]     = %p\n", idxPast, pStreams[idx].ppPastSurfaces[idxPast]);fflush(stderr);
        }

        printf("    FutureFrames       = %i\n", pStreams[idx].FutureFrames);fflush(stderr);
        printf("    ppFutureSurfaces   = %p\n", pStreams[idx].ppFutureSurfaces);fflush(stderr);
        for( int idxNext = 0; idxNext < (int)pStreams->FutureFrames; idxNext++ )
        {
            printf("        ppFutureSurfaces[%i]     = %p\n", idxNext, pStreams[idx].ppFutureSurfaces[idxNext]);fflush(stderr);
        }

        printf("    pInputSurface      = %p\n", pStreams[idx].pInputSurface);fflush(stderr);

        printf("    ppPastSurfacesRight= %p\n", pStreams[idx].ppPastSurfacesRight);fflush(stderr);
        printf("    pInputSurfaceRight = %p\n", pStreams[idx].pInputSurfaceRight);fflush(stderr);
        printf("    ppFutureSurfacesRight   = %p\n", pStreams[idx].ppFutureSurfacesRight);fflush(stderr);
    }
#endif
    //-----------------------------------------------------

    return hRes;

} // HRESULT D3D11VideoProcessor::VideoProcessorBlt(...)


mfxStatus D3D11VideoProcessor::Register(mfxHDLPair* pSurfaces, mfxU32 num, BOOL bRegister)
{
    pSurfaces; num; bRegister;

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoProcessor::Register(mfxHDL* pSurfaces, mfxU32 num, BOOL bRegister)


mfxStatus D3D11VideoProcessor::QueryCapabilities(mfxVppCaps& caps)
{
    caps.uDenoiseFilter = m_caps.m_denoiseEnable;
    caps.uSimpleDI      = m_caps.m_simpleDIEnable;
    caps.uAdvancedDI    = m_caps.m_advancedDIEnable;
    caps.uDetailFilter  = m_caps.m_detailEnable;
    caps.uProcampFilter = m_caps.m_procAmpEnable;
    caps.uRotation      = m_caps.m_rotation;
    caps.uIStabFilter   = m_vpreCaps.bIS;
    caps.uVariance      = m_vpreCaps.bVariance;
    caps.uScaling       = m_vpreCaps.bScalingMode;
    if(caps.uVariance)
    {
        caps.iNumBackwardSamples = 1; // from the spec 1.8
    }

    // [Max Resolution]
    caps.uMaxWidth = 16352;
    caps.uMaxHeight = 16352;

#ifdef VPP_MIRRORING
    caps.uMirroring = 1; // Mirroring supported thru special CM copy kernel
#endif

    if( TRUE == m_vpreCaps.bFieldWeavingControl )
    {
        caps.uFieldWeavingControl = 1;
    }

    // [FRC]
    size_t rateDataCount = m_customRateData.size();
    // aya: wo
    //rateDataCount = 0;

    if(rateDataCount > 0)
    {
        caps.uFrameRateConversion = 1;
        caps.frcCaps.customRateData.resize(rateDataCount);

        for(mfxU32 indx = 0; indx < rateDataCount; indx++ )
        {
            /*caps.frcCaps.customRateData[indx].bkwdRefCount     = m_frcCaps.sCustomRateData[indx].usNumBackwardSamples;
            caps.frcCaps.customRateData[indx].fwdRefCount      = m_frcCaps.sCustomRateData[indx].usNumForwardSamples;
            caps.frcCaps.customRateData[indx].inputFramesCount = m_frcCaps.sCustomRateData[indx].InputFramesOrFields;
            caps.frcCaps.customRateData[indx].outputFramesCount= m_frcCaps.sCustomRateData[indx].OutputFrames;
            caps.frcCaps.customRateData[indx].customRate.FrameRateExtN = m_frcCaps.sCustomRateData[indx].CustomRate.Numerator;
            caps.frcCaps.customRateData[indx].customRate.FrameRateExtD = m_frcCaps.sCustomRateData[indx].CustomRate.Denominator;*/
            caps.frcCaps.customRateData[indx] = m_customRateData[indx];
        }
    }

    caps.cameraCaps.uBlackLevelCorrection  = m_cameraCaps.bBlackLevelCorrection;
    caps.cameraCaps.uColorCorrectionMatrix = m_cameraCaps.bColorCorrection;
    caps.cameraCaps.uGammaCorrection       = m_cameraCaps.bForwardGamma;
    caps.cameraCaps.uHotPixelCheck         = m_cameraCaps.bHotPixel;
    caps.cameraCaps.uVignetteCorrection    = m_cameraCaps.bVignetteCorrection;
    caps.cameraCaps.uWhiteBalance          = m_cameraCaps.bWhiteBalanceManual;
    caps.cameraCaps.u3DLUT                 = m_cameraCaps.b3DLUT;
    // [FourCC]
    for (mfxU32 indx = 0; indx < sizeof(g_TABLE_SUPPORTED_FOURCC)/sizeof(mfxU32); indx++)
    {
        caps.mFormatSupport[g_TABLE_SUPPORTED_FOURCC[indx]] = m_caps.m_formatSupport[mfxDefaultAllocatorD3D11::MFXtoDXGI(g_TABLE_SUPPORTED_FOURCC[indx])];
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoProcessor::QueryCapabilities(mfxVppCaps& caps)

void MfxVppDump::WriteFrameData(
    vm_file *            file,
    VideoCORE *          core,
    mfxFrameData const & fdata,
    mfxFrameInfo const & info,
    bool external)
{
    mfxFrameData data = fdata;
    FrameLocker lock(core, data, external);

    if (file != 0 && data.Y != 0 && data.UV != 0)
    {
        for (mfxU32 i = 0; i < info.Height; i++)
            vm_file_fwrite(data.Y + i * data.Pitch, 1, info.Width, file);

        for (mfxI32 y = 0; y < info.Height / 2; y++)
            vm_file_fwrite(data.UV + y * data.Pitch, 1, info.Width, file);

        vm_file_fflush(file);
    }

} // void WriteFrameData(...)


#endif
