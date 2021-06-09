// Copyright (c) 2007-2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if defined  (MFX_D3D11_ENABLED)
#include <windows.h>
#include <initguid.h>
#include <DXGI.h>

#include "mfxpcp.h"
#include <libmfx_core_d3d11.h>
#include <libmfx_core_d3d9.h>
#include "mfx_utils.h"
#include "mfx_session.h"
#include "libmfx_core_interface.h"
#include "umc_va_dxva2_protected.h"
#include "ippi.h"
#include "ippcc.h"

#include "mfx_umc_alloc_wrapper.h"
#include "mfx_common_decode_int.h"
#include "libmfx_core_hw.h"

#include "cm_mem_copy.h"

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report,
            0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);

template <class Base>
D3D11VideoCORE_T<Base>::D3D11VideoCORE_T(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session)
    :   Base(numThreadsAvailable, session)
    ,   m_pid3d11Adapter(nullptr)
    ,   m_bUseExtAllocForHWFrames(false)
    ,   m_pAccelerator(nullptr)
    ,   m_adapterNum(adapterNum)
    ,   m_HWType(MFX_HW_UNKNOWN)
    ,   m_GTConfig(MFX_GT_UNKNOWN)
    ,   m_bCmCopy(false)
    ,   m_bCmCopySwap(false)
    ,   m_bCmCopyAllowed(true)
    ,   m_VideoDecoderConfigCount(0)
    ,   m_Configs()
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    ,   m_bIsBlockingTaskSyncEnabled(false)
#endif
{
}

template <class Base>
D3D11VideoCORE_T<Base>::~D3D11VideoCORE_T()
{
    if (m_bCmCopy)
    {
        m_pCmCopy->Release();
        m_bCmCopy = false;
        m_bCmCopySwap = false;
    }
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::InternalInit()
{
    if (m_HWType != MFX_HW_UNKNOWN)
        return MFX_ERR_NONE;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE_T<Base>::InternalInit");

    mfxU32 platformFromDriver = 0;

    D3D11_VIDEO_DECODER_CONFIG config;
    mfxStatus sts = GetIntelDataPrivateReport(DXVA2_ModeH264_VLD_NoFGT, 0, config);
    if (sts == MFX_ERR_NONE)
        platformFromDriver = config.ConfigBitstreamRaw;

    //need to replace with specific D3D11 approach
    m_HWType = MFX::GetHardwareType(m_adapterNum, platformFromDriver);

#ifndef STRIP_EMBARGO
    // Temporary disable CmCopy for Pre-Si platforms or if there is no loaded cm_copy_kernel at the moment
    if (   m_HWType == MFX_HW_RYF
        || m_HWType == MFX_HW_RKL
        || m_HWType == MFX_HW_DG2
        || m_HWType == MFX_HW_MTL
        || m_HWType == MFX_HW_ELG)
        m_bCmCopyAllowed = false;
#endif

    m_deviceId = MFX::GetDeviceId(m_adapterNum);

    return MFX_ERR_NONE;
}

template <class Base>
eMFXHWType D3D11VideoCORE_T<Base>::GetHWType()
{
    std::ignore = MFX_STS_TRACE(InternalInit());

    return m_HWType;
}

template <class Base>
D3D11_VIDEO_DECODER_CONFIG* D3D11VideoCORE_T<Base>::GetConfig(D3D11_VIDEO_DECODER_DESC *video_desc, mfxU32 start, mfxU32 end, const GUID guid)
{
    HRESULT hr;
    for (mfxU32 i = start; i < end; i++)
    {
        D3D11_VIDEO_DECODER_CONFIG Config = {};
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfig");
            hr = m_pD11VideoDevice->GetVideoDecoderConfig(video_desc, i, &Config);
        }

        if (FAILED(hr))
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_UNDEFINED_BEHAVIOR);
            return nullptr;
        }

        m_Configs.push_back(Config);

        if (m_Configs[i].guidConfigBitstreamEncryption == guid)
        {
            return &m_Configs[i];
        }
    }
    return nullptr;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::GetIntelDataPrivateReport(const GUID guid, mfxVideoParam *par, D3D11_VIDEO_DECODER_CONFIG & config)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE_T<Base>::GetIntelDataPrivateReport");
    mfxStatus mfxRes = InternalCreateDevice();
    MFX_CHECK_STS(mfxRes);

    D3D11_VIDEO_DECODER_DESC video_desc = {0};
    video_desc.Guid         = DXVADDI_Intel_Decode_PrivateData_Report;
    video_desc.SampleWidth  = par && par->mfx.FrameInfo.Width  ? par->mfx.FrameInfo.Width  : 640;
    video_desc.SampleHeight = par && par->mfx.FrameInfo.Height ? par->mfx.FrameInfo.Height : 480;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;

    mfxU32 cDecoderProfiles = 0;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfileCount");
        cDecoderProfiles = m_pD11VideoDevice->GetVideoDecoderProfileCount();
    }
    bool isRequestedGuidPresent = false;
    bool isIntelGuidPresent = false;

    for (mfxU32 i = 0; i < cDecoderProfiles; i++)
    {
        GUID decoderGuid;
        HRESULT hr;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfile");
            hr = m_pD11VideoDevice->GetVideoDecoderProfile(i, &decoderGuid);
        }
        if (FAILED(hr))
        {
            continue;
        }

        if (guid == decoderGuid)
            isRequestedGuidPresent = true;

        if (DXVADDI_Intel_Decode_PrivateData_Report == decoderGuid)
            isIntelGuidPresent = true;
    }

    MFX_CHECK(isRequestedGuidPresent, MFX_ERR_NOT_FOUND);
    // if no required GUID - no acceleration at all
    MFX_CHECK(isIntelGuidPresent,     MFX_WRN_PARTIAL_ACCELERATION);

    UMC::AutomaticUMCMutex lock(m_guard); // protects m_Configs

    // NOTE: the following functions GetVideoDecoderConfigCount and GetVideoDecoderConfig
    // take too much time (~230us and ~560us respectively). So we cache these values.
    if (0 == m_VideoDecoderConfigCount)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfigCount");
        HRESULT hr = m_pD11VideoDevice->GetVideoDecoderConfigCount(&video_desc, &m_VideoDecoderConfigCount);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    auto it = std::find_if(std::begin(m_Configs), std::end(m_Configs),
        [&guid](const D3D11_VIDEO_DECODER_CONFIG& config) { return config.guidConfigBitstreamEncryption == guid; });

    D3D11_VIDEO_DECODER_CONFIG *video_config = it != std::end(m_Configs) ?
        &(*it) : GetConfig(&video_desc, (mfxU32)m_Configs.size(), m_VideoDecoderConfigCount, guid);

    MFX_CHECK(video_config, MFX_WRN_PARTIAL_ACCELERATION);

    config = *video_config;

    if (guid == DXVA2_Intel_Encode_AVC && video_config->ConfigSpatialResid8 != INTEL_AVC_ENCODE_DDI_VERSION)
    {
        MFX_RETURN(MFX_WRN_PARTIAL_ACCELERATION);
    }

    if (guid == DXVA2_Intel_Encode_MPEG2 && video_config->ConfigSpatialResid8 != INTEL_MPEG2_ENCODE_DDI_VERSION)
    {
        MFX_RETURN(MFX_WRN_PARTIAL_ACCELERATION);
    }

    if (guid == DXVA2_Intel_Encode_JPEG  && video_config->ConfigSpatialResid8 != INTEL_MJPEG_ENCODE_DDI_VERSION)
    {
        MFX_RETURN(MFX_WRN_PARTIAL_ACCELERATION);
    }

    /*if (guid == DXVA2_Intel_Encode_HEVC_Main)
    {
    m_HEVCEncodeDDIVersion = video_config->ConfigSpatialResid8;
    }*/

    return  MFX_ERR_NONE;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncode)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE_T<Base>::IsGuidSupported");
    MFX_CHECK(par, MFX_WRN_PARTIAL_ACCELERATION);

    mfxStatus sts = InternalInit();
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

    sts = InternalCreateDevice();
    MFX_CHECK(sts >= MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

    D3D11_VIDEO_DECODER_CONFIG config;
    sts = GetIntelDataPrivateReport(guid, par, config);
    MFX_CHECK(sts >= MFX_ERR_NONE && sts != MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION);

    if (sts == MFX_ERR_NONE && !isEncode)
    {
        return CheckIntelDataPrivateReport<D3D11_VIDEO_DECODER_CONFIG>(&config, par);
    }

    return MFX_ERR_NONE;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::InitializeDevice(bool isTemporal)
{
    mfxStatus sts = InternalInit();
    MFX_CHECK_STS(sts);

    sts = InternalCreateDevice();
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    if (m_pD11Device && !m_hdl && !isTemporal)
    {
        m_hdl = (mfxHDL)m_pD11Device;
    }
    return MFX_ERR_NONE;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::InternalCreateDevice()
{
    if (m_pD11Device)
        return MFX_ERR_NONE;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE_T<Base>::InternalCreateDevice");

    HRESULT hres = S_OK;
    static D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    D3D_DRIVER_TYPE type = D3D_DRIVER_TYPE_HARDWARE;
    m_pAdapter = nullptr;

    if (m_adapterNum != 0) // not default
    {
        IDXGIFactory * pFactory = nullptr;
        hres = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**) (&pFactory));
        MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

        // Release owned factory (if exists) and take ownership without Addref
        m_pFactory.Attach(pFactory);

        IDXGIAdapter* pAdapter = nullptr;
        hres = m_pFactory->EnumAdapters(m_adapterNum, &pAdapter);
        MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

        // Release owned adapter (if exists) and take ownership without Addref
        m_pAdapter.Attach(pAdapter);

        type = D3D_DRIVER_TYPE_UNKNOWN; // !!!!!! See MSDN, how to create device using not default device
    }

    CComPtr<ID3D11Device>         pD11Device;
    CComPtr<ID3D11DeviceContext>  pD11Context;

    hres = D3D11CreateDevice(m_pAdapter,    // provide real adapter
        type,
        nullptr,
        0,
        FeatureLevels,
        sizeof(FeatureLevels)/sizeof(D3D_FEATURE_LEVEL),
        D3D11_SDK_VERSION,
        &pD11Device,
        &pFeatureLevelsOut,
        &pD11Context);

    if (FAILED(hres))
    {
        pD11Device.Release();
        pD11Context.Release();

        // lets try to create dx11 device with d9 feature level
        static D3D_FEATURE_LEVEL FeatureLevels9[] = {
            D3D_FEATURE_LEVEL_9_1,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_3 };

        hres = D3D11CreateDevice(m_pAdapter,    // provide real adapter
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            FeatureLevels9,
            sizeof(FeatureLevels9)/sizeof(D3D_FEATURE_LEVEL),
            D3D11_SDK_VERSION,
            &pD11Device,
            &pFeatureLevelsOut,
            &pD11Context);

        MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);
    }

    // At this point following pointers should be valid, so store them at class variables
    m_pD11Device  = pD11Device;
    m_pD11Context = pD11Context;

    // default device should be thread safe
    CComQIPtr<ID3D10Multithread> p_mt(m_pD11Context);
    MFX_CHECK(p_mt, MFX_ERR_DEVICE_FAILED);

    p_mt->SetMultithreadProtected(true);

    m_pD11VideoDevice = m_pD11Device;
    MFX_CHECK(m_pD11VideoDevice, MFX_ERR_DEVICE_FAILED);

    m_pD11VideoContext = m_pD11Context;
    MFX_CHECK(m_pD11VideoContext, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoCORE_T<Base>::CreateDevice()

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::AllocFrames(mfxFrameAllocRequest *request,
                                      mfxFrameAllocResponse *response, bool isNeedCopy)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE_T<Base>::AllocFrames");

    MFX_CHECK_NULL_PTR2(request, response);

    UMC::AutomaticUMCMutex guard(m_guard);
    try
    {
        // external allocator doesn't know how to allocate opaque surfaces
        // we can treat opaque as internal
        if (    request->Info.FourCC == MFX_FOURCC_P8
#if defined(MFX_ENABLE_OPAQUE_MEMORY)
            || (request->Type & MFX_MEMTYPE_OPAQUE_FRAME)
#endif
            // use internal allocator for R16 since creating requires
            // Intel's internal resource extensions that are not
            // exposed for external application
            || IsBayerFormat(request->Info.FourCC))
        {
            request->Type &= ~MFX_MEMTYPE_EXTERNAL_FRAME;
            request->Type |= MFX_MEMTYPE_INTERNAL_FRAME;
        }

        // Create Service - first call
        mfxStatus sts = InitializeDevice();
        MFX_CHECK_STS(sts);

        if (!m_bCmCopy && m_bCmCopyAllowed && isNeedCopy)
        {
            m_pCmCopy.reset(new CmCopyWrapper);

            if (!m_pCmCopy->GetCmDevice<ID3D11Device>(m_pD11Device))
            {
                //!!!! WA: CM restricts multiple CmDevice creation from different device managers.
                //if failed to create CM device, continue without CmCopy
                m_bCmCopy        = false;
                m_bCmCopyAllowed = false;

                m_pCmCopy.reset();
                //return MFX_ERR_DEVICE_FAILED;
            }
            else
            {
                sts = m_pCmCopy->Initialize(GetHWType());
                MFX_CHECK_STS(sts);
                m_bCmCopy = true;
            }
        }
        else if (m_bCmCopy)
        {
            if (m_pCmCopy)
                m_pCmCopy->ReleaseCmSurfaces();
            else
                m_bCmCopy = false;
        }
        if (m_pCmCopy && !m_bCmCopySwap && (request->Info.FourCC == MFX_FOURCC_BGR4 || request->Info.FourCC == MFX_FOURCC_RGB4 || request->Info.FourCC == MFX_FOURCC_ARGB16 || request->Info.FourCC == MFX_FOURCC_ARGB16 || request->Info.FourCC == MFX_FOURCC_P010))
        {
            sts = m_pCmCopy->InitializeSwapKernels(GetHWType());
            m_bCmCopySwap = true;
        }

        // use common core for sw surface allocation
        if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            return Base::AllocFrames(request, response);
        else
        {
            // make 'fake' Alloc call to retrieve memId's of surfaces already allocated by app
            bool isExtAllocatorCallAllowed = (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) && (request->Type & MFX_MEMTYPE_FROM_DECODE);
            // external allocator
            if (m_bSetExtFrameAlloc && isExtAllocatorCallAllowed)
            {
                sts = (*m_FrameAllocator.frameAllocator.Alloc)(m_FrameAllocator.frameAllocator.pthis,request, response);
                MFX_CHECK_STS(sts);

                // let's create video accelerator
                m_bUseExtAllocForHWFrames = true;
                RegisterMids(response, request->Type, false);
                if (response->NumFrameActual < request->NumFrameMin)
                {
                    (*m_FrameAllocator.frameAllocator.Free)(m_FrameAllocator.frameAllocator.pthis, response);
                    return MFX_ERR_MEMORY_ALLOC;
                }
                return sts;
            }
            else
            {
                // Default Allocator is used for internal memory allocation only
                m_bUseExtAllocForHWFrames = false;
                sts = DefaultAllocFrames(request, response);
                MFX_CHECK_STS(sts);

                return sts;
            }
        }
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }
} // mfxStatus D3D11VideoCORE_T<Base>::AllocFrames

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::ReallocFrame(mfxFrameSurface1 *surf)
{
    MFX_CHECK_NULL_PTR1(surf);

    mfxMemId memid = surf->Data.MemId;

    if (!(surf->Data.MemType & MFX_MEMTYPE_INTERNAL_FRAME &&
        (!(surf->Data.MemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET) ||
        !(surf->Data.MemType & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET))))
        return MFX_ERR_MEMORY_ALLOC;

    mfxFrameAllocator *pFrameAlloc = GetAllocatorAndMid(memid);
    if (!pFrameAlloc)
        return MFX_ERR_MEMORY_ALLOC;

    return mfxDefaultAllocatorD3D11::ReallocFrameHW(pFrameAlloc->pthis, memid, &(surf->Info));
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts = MFX_ERR_NONE;

    if ((request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET)||
        (request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) // SW - TBD !!!!!!!!!!!!!!
    {
        // Create Service - first call
        sts = InitializeDevice();
        MFX_CHECK_STS(sts);
        mfxBaseWideFrameAllocator* pAlloc = GetAllocatorByReq(request->Type);

        // VPP, ENC, PAK can request frames for several times
        if (pAlloc && (request->Type & MFX_MEMTYPE_FROM_DECODE))
            return MFX_ERR_MEMORY_ALLOC;

        if (!pAlloc)
        {
            m_pcHWAlloc.reset(new mfxDefaultAllocatorD3D11::mfxWideHWFrameAllocator(request->Type,
                m_pD11Device,
                m_pD11Context));
            pAlloc = m_pcHWAlloc.get();
        }
        // else ???

        pAlloc->frameAllocator.pthis = pAlloc;
        sts = (*pAlloc->frameAllocator.Alloc)(pAlloc->frameAllocator.pthis,request, response);
        MFX_CHECK_STS(sts);
        sts = ProcessRenderTargets(request, response, pAlloc);
        MFX_CHECK_STS(sts);

    }
    else
    {
        return Base::DefaultAllocFrames(request, response);
    }
    ++m_NumAllocators;
    return sts;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::CreateVA(mfxVideoParam *param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, UMC::FrameAllocator *allocator)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE_T<Base>::CreateVA");
    mfxStatus sts = MFX_ERR_NONE;

    UMC::VideoStreamInfo vi{};
    ConvertMFXParamsToUMC(param, &vi);

    MFXD3D11AcceleratorParams params{};
    params.m_protectedVA      = param->Protected;
    params.m_pVideoStreamInfo = &vi;
    params.m_allocator        = allocator;
    params.m_iNumberSurfaces  = request->NumFrameMin;
    params.m_video_device     = m_pD11VideoDevice;
    params.m_video_context    = m_pD11VideoContext;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    params.m_video_processing =
        reinterpret_cast<mfxExtDecVideoProcessing*>(GetExtendedBuffer(param->ExtParam, param->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING));
#endif

    m_pAccelerator.reset(new MFXD3D11Accelerator());

    auto const profile = ChooseProfile(param, GetHWType());
    MFX_CHECK(profile, MFX_ERR_UNSUPPORTED);

    m_pAccelerator->m_Profile = static_cast<UMC::VideoAccelerationProfile>(profile);

    sts = ConvertUMCStatusToMfx(m_pAccelerator->Init(&params));
    MFX_CHECK_STS(sts);

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
    auto pScheduler = (MFXIScheduler2 *)m_session->m_pScheduler->QueryInterface(MFXIScheduler2_GUID);
    MFX_CHECK(pScheduler, MFX_ERR_UNDEFINED_BEHAVIOR);

    m_pAccelerator->SetGlobalHwEvent(pScheduler->GetHwEvent());
    pScheduler->Release();
#endif

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (IS_PROTECTION_ANY(param->Protected) && !IS_PROTECTION_CENC(param->Protected))
    {
        sts = ConvertUMCStatusToMfx(m_pAccelerator->GetVideoDecoderDriverHandle(&m_DXVA2DecodeHandle));
        if (sts != MFX_ERR_NONE)
        {
            m_pAccelerator->Close();
            MFX_RETURN(sts);
        }
    }
    else
#endif
        m_DXVA2DecodeHandle = nullptr;

    m_pAccelerator->GetVideoDecoder(&m_D3DDecodeHandle);
    m_pAccelerator->m_HWPlatform = m_HWType;

    return sts;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::CreateVideoProcessing(mfxVideoParam *)
{
#if defined (MFX_ENABLE_VPP)
    if (!m_vpp_hw_resmng.GetDevice()){
        return m_vpp_hw_resmng.CreateDevice(this);
    }
    return MFX_ERR_NONE;
#else
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
#endif
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc)
{
    RegisterMids(response, request->Type, !m_bUseExtAllocForHWFrames, pAlloc);
    m_pcHWAlloc.release();
    return MFX_ERR_NONE;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::SetCmCopyStatus(bool enable)
{
    m_bCmCopyAllowed = enable;
    if (!enable)
    {
        m_pCmCopy.reset();

        m_bCmCopy = false;
    }
    return MFX_ERR_NONE;
}

template <class Base>
void* D3D11VideoCORE_T<Base>::QueryCoreInterface(const MFX_GUID &guid)
{
    if (MFXICORED3D11_GUID == guid)
    {
        if (!m_pid3d11Adapter)
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            m_pid3d11Adapter.reset(new D3D11Adapter(this));
        }
        return (void*)m_pid3d11Adapter.get();
    }

    if (MFXICORE_GT_CONFIG_GUID == guid)
    {
        return (void*)&m_GTConfig;
    }

    if (MFXIHWCAPS_GUID == guid)
    {
        return (void*) &m_encode_caps;
    }

    if (MFXIHWMBPROCRATE_GUID == guid)
    {
        return (void*) &m_encode_mbprocrate;
    }

    if (MFXID3D11DECODER_GUID == guid)
    {
        return (void*)&m_comptr;
    }

    if (MFXICORECM_GUID == guid)
    {
        CmDevice* pCmDevice = nullptr;
        if (!m_bCmCopy)
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            m_pCmCopy.reset(new CmCopyWrapper);
            pCmDevice = m_pCmCopy->GetCmDevice<ID3D11Device>(m_pD11Device);

            if (!pCmDevice)
            {
                std::ignore = MFX_STS_TRACE(MFX_ERR_NULL_PTR);
                return nullptr;
            }

            mfxStatus sts = m_pCmCopy->Initialize(GetHWType());
            if (MFX_ERR_NONE != MFX_STS_TRACE(sts))
                return nullptr;

            m_bCmCopy = true;
        }
        else
        {
            pCmDevice =  m_pCmCopy->GetCmDevice<ID3D11Device>(m_pD11Device);
        }
        return (void*)pCmDevice;
    }

    if (MFXICORECMCOPYWRAPPER_GUID == guid)
    {
        if (!m_pCmCopy)
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            m_pCmCopy.reset(new CmCopyWrapper);

            if (!m_pCmCopy->GetCmDevice<ID3D11Device>(m_pD11Device)){
                //!!!! WA: CM restricts multiple CmDevice creation from different device managers.
                //if failed to create CM device, continue without CmCopy
                m_bCmCopy        = false;
                m_bCmCopyAllowed = false;

                m_pCmCopy.reset();

                return nullptr;
            }

            mfxStatus sts = m_pCmCopy->Initialize(GetHWType());
            if (MFX_ERR_NONE != MFX_STS_TRACE(sts))
                return nullptr;

            m_bCmCopy = true;
        }

        return (void*)m_pCmCopy.get();
    }

    if (MFXICMEnabledCore_GUID == guid)
    {
        if (!m_pCmAdapter)
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            m_pCmAdapter.reset(new CMEnabledCoreAdapter(this));
        }
        return (void*)m_pCmAdapter.get();
    }

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    if (MFXBlockingTaskSyncEnabled_GUID == guid)
    {
        m_bIsBlockingTaskSyncEnabled = m_HWType > MFX_HW_SCL;
        return &m_bIsBlockingTaskSyncEnabled;
    }
#endif

    return Base::QueryCoreInterface(guid);
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE_T<Base>::DoFastCopyWrapper");
    mfxStatus sts;

    mfxHDLPair srcHandle = {}, dstHandle = {};
    mfxMemId srcMemId, dstMemId;

    mfxFrameSurface1 srcTempSurface, dstTempSurface;

    memset(&srcTempSurface, 0, sizeof(mfxFrameSurface1));
    memset(&dstTempSurface, 0, sizeof(mfxFrameSurface1));

    // save original mem ids
    srcMemId = pSrc->Data.MemId;
    dstMemId = pDst->Data.MemId;

    mfxU8* srcPtr = GetFramePointer(pSrc->Info.FourCC, pSrc->Data);
    mfxU8* dstPtr = GetFramePointer(pDst->Info.FourCC, pDst->Data);

    srcTempSurface.Info = pSrc->Info;
    dstTempSurface.Info = pDst->Info;

    bool isSrcLocked = false;
    bool isDstLocked = false;

    if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == srcPtr)
            {
                sts = LockExternalFrame(srcMemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);

                isSrcLocked = true;
            }
            else
            {
                srcTempSurface.Data = pSrc->Data;
                srcTempSurface.Data.MemId = 0;
            }
        }
        else if (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetExternalFrameHDL(srcMemId, (mfxHDL*)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
        }
    }
    else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == srcPtr)
            {
                sts = LockFrame(srcMemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);

                isSrcLocked = true;
            }
            else
            {
                srcTempSurface.Data = pSrc->Data;
                srcTempSurface.Data.MemId = 0;
            }
        }
        else if (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetFrameHDL(srcMemId, (mfxHDL*)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
        }
    }

    if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == dstPtr)
            {
                sts = LockExternalFrame(dstMemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);

                isDstLocked = true;
            }
            else
            {
                dstTempSurface.Data = pDst->Data;
                dstTempSurface.Data.MemId = 0;
            }
        }
        else if (dstMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetExternalFrameHDL(dstMemId, (mfxHDL*)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
        }
    }
    else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == dstPtr)
            {
                sts = LockFrame(dstMemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);

                isDstLocked = true;
            }
            else
            {
                dstTempSurface.Data = pDst->Data;
                dstTempSurface.Data.MemId = 0;
            }
        }
        else if (dstMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetFrameHDL(dstMemId, (mfxHDL*)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
        }
    }

    mfxStatus fcSts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface);

    if (true == isSrcLocked)
    {
        if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = UnlockExternalFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
        else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = UnlockFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
    }

    if (true == isDstLocked)
    {
        if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = UnlockExternalFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
        else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = UnlockFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
    }

    return fcSts;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::ConvertYV12toNV12SW(mfxFrameSurface1* pDst, mfxFrameSurface1* pSrc)
{
    MFX_CHECK_NULL_PTR2(pDst, pSrc);
    MFX_CHECK(pSrc->Info.FourCC == MFX_FOURCC_YV12, MFX_ERR_NONE);
    MFX_CHECK(!(pSrc->Info.Width > pDst->Info.Width ||
        pSrc->Info.Height > pDst->Info.Height), MFX_ERR_MEMORY_ALLOC);

    mfxU16 inW = pSrc->Info.CropX + pSrc->Info.CropW;
    mfxU16 inH = pSrc->Info.CropY + pSrc->Info.CropH;

    const Ipp8u *(pSource[3]) = { (Ipp8u*)pSrc->Data.Y,
                          (Ipp8u*)pSrc->Data.U,
                          (Ipp8u*)pSrc->Data.V };

    int pSrcStep[3] = { pSrc->Data.Pitch,
                       pSrc->Data.Pitch / 2,
                       pSrc->Data.Pitch / 2 };

    Ipp8u *pDstY = (Ipp8u*)pDst->Data.Y;
    Ipp8u *pDstUV = (Ipp8u*)pDst->Data.UV;
    Ipp32u pDstYStep = pDst->Data.Pitch;
    Ipp32u pDstUVStep = pDst->Data.Pitch;

    IppiSize roiSize = { inW, inH };

    IppStatus status;
    status = ippiYCbCr420_8u_P3P2R(pSource, pSrcStep, pDstY, pDstYStep, pDstUV, pDstUVStep, roiSize);
    VM_ASSERT(ippStsNoErr == status);

    mfxFrameData tmp = pSrc->Data;
    pSrc->Data = pDst->Data;
    pDst->Data = tmp;

    return MFX_ERR_NONE;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU8* srcPtr;
    mfxU8* dstPtr;
    pDst; pSrc;

    sts = GetFramePointerChecked(pSrc->Info, pSrc->Data, &srcPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);
    sts = GetFramePointerChecked(pDst->Info, pDst->Data, &dstPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    // check that only memId or pointer are passed
    // otherwise don't know which type of memory copying is requested
    if (
        (NULL != dstPtr && NULL != pDst->Data.MemId) ||
        (NULL != srcPtr && NULL != pSrc->Data.MemId)
        )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    IppiSize roi = {IPP_MIN(pSrc->Info.Width, pDst->Info.Width), IPP_MIN(pSrc->Info.Height, pDst->Info.Height)};

    // check that region of interest is valid
    if (0 == roi.width || 0 == roi.height)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    HRESULT hRes;

    D3D11_TEXTURE2D_DESC sSurfDesc = {0};
    D3D11_MAPPED_SUBRESOURCE sLockRect = {0};

    mfxU32 srcPitch = pSrc->Data.PitchLow + ((mfxU32)pSrc->Data.PitchHigh << 16);
    mfxU32 dstPitch = pDst->Data.PitchLow + ((mfxU32)pDst->Data.PitchHigh << 16);

    bool canUseCMCopy = m_bCmCopy ? CmCopyWrapper::CanUseCmCopy(pDst, pSrc) : false;

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopyVideoToVideo(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
        {
            ID3D11Texture2D * pSurfaceSrc = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pSrc->Data.MemId)->first);
            ID3D11Texture2D * pSurfaceDst = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pDst->Data.MemId)->first);

            size_t indexSrc = (size_t)((mfxHDLPair*)pSrc->Data.MemId)->second;
            size_t indexDst = (size_t)((mfxHDLPair*)pDst->Data.MemId)->second;


            m_pD11Context->CopySubresourceRegion(pSurfaceDst, (mfxU32)indexDst, 0, 0, 0, pSurfaceSrc, (mfxU32)indexSrc, NULL);
        }

    }
    else if (NULL != pSrc->Data.MemId && NULL != dstPtr)
    {
        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopyVideoToSys(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
        {
            ID3D11Texture2D * pSurface = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pSrc->Data.MemId)->first);

            pSurface->GetDesc(&sSurfDesc);

            D3D11_TEXTURE2D_DESC desc = {0};

            desc.Width = sSurfDesc.Width;
            desc.Height = sSurfDesc.Height;
            desc.MipLevels = 1;
            desc.Format = sSurfDesc.Format;
            desc.SampleDesc.Count = 1;
            desc.ArraySize = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.BindFlags = 0;
            ID3D11Texture2D *pStaging;
            if ( DXGI_FORMAT_R16_TYPELESS == sSurfDesc.Format )
            {
                RESOURCE_EXTENSION extnDesc;
                ZeroMemory( &extnDesc, sizeof(RESOURCE_EXTENSION) );
                static_assert (sizeof RESOURCE_EXTENSION_KEY <= sizeof extnDesc.Key,
                        "sizeof RESOURCE_EXTENSION_KEY > sizeof extnDesc.Key");
                std::copy(std::begin(RESOURCE_EXTENSION_KEY), std::end(RESOURCE_EXTENSION_KEY), extnDesc.Key);
                extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
                extnDesc.Type    = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
                extnDesc.Data[0] = BayerFourCC2FormatFlag(pDst->Info.FourCC);

                hRes = SetResourceExtension(m_pD11Device, &extnDesc);
            }
            hRes = m_pD11Device->CreateTexture2D(&desc, NULL, &pStaging);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_MEMORY_ALLOC);

            size_t index = (size_t)((mfxHDLPair*)pSrc->Data.MemId)->second;

            m_pD11Context->CopySubresourceRegion(pStaging, 0, 0, 0, 0, pSurface, (mfxU32)index, NULL);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Dx11 Core Fast Copy SSE");

            do
            {
                hRes = m_pD11Context->Map(pStaging, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &sLockRect);
            }
            while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);

            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

            srcPitch = sLockRect.RowPitch;
            sts = mfxDefaultAllocatorD3D11::SetFrameData(sSurfDesc, sLockRect, pSrc->Data);
            MFX_CHECK_STS(sts);

            mfxMemId saveMemId = pSrc->Data.MemId;
            pSrc->Data.MemId = 0;

            if (pDst->Info.FourCC == DXGI_FORMAT_AYUV)
                pDst->Info.FourCC = MFX_FOURCC_AYUV;

            sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_VIDEO_TO_SYS); // sw copy
            MFX_CHECK_STS(sts);

            pSrc->Data.MemId = saveMemId;
            MFX_CHECK_STS(sts);

            m_pD11Context->Unmap(pStaging, 0);

            SAFE_RELEASE(pStaging);
        }
    }
    else if (NULL != srcPtr && NULL != dstPtr)
    {
        // system memories were passed
        // use common way to copy frames

        if (pDst->Info.FourCC == DXGI_FORMAT_AYUV)
            pDst->Info.FourCC = MFX_FOURCC_AYUV;

        sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_SYS); // sw copy
        MFX_CHECK_STS(sts);

    }
    else if (NULL != srcPtr && NULL != pDst->Data.MemId)
    {
        // source are placed in system memory, destination is in video memory
        // use common way to copy frames from system to video, most faster
        mfxI64 verticalPitch = (mfxI64)(pSrc->Data.UV - pSrc->Data.Y);
        verticalPitch = (verticalPitch % pSrc->Data.Pitch)? 0 : verticalPitch / pSrc->Data.Pitch;
        if ( IsBayerFormat(pSrc->Info.FourCC) )
        {
            // Only one plane is used for Bayer and vertical pitch calculation is not correct for it.
            verticalPitch = pDst->Info.Height;
        }

        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopySysToVideo(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
        {
            ID3D11Texture2D * pSurface = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pDst->Data.MemId)->first);

            pSurface->GetDesc(&sSurfDesc);

            D3D11_TEXTURE2D_DESC desc = {0};

            desc.Width = sSurfDesc.Width;
            desc.Height = sSurfDesc.Height;
            desc.MipLevels = 1;
            desc.Format = sSurfDesc.Format;
            desc.SampleDesc.Count = 1;
            desc.ArraySize = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = 0;
            if ( DXGI_FORMAT_R16_TYPELESS == sSurfDesc.Format )
            {
                RESOURCE_EXTENSION extnDesc;
                ZeroMemory( &extnDesc, sizeof(RESOURCE_EXTENSION) );
                static_assert (sizeof RESOURCE_EXTENSION_KEY <= sizeof extnDesc.Key,
                        "sizeof RESOURCE_EXTENSION_KEY > sizeof extnDesc.Key");
                std::copy(std::begin(RESOURCE_EXTENSION_KEY), std::end(RESOURCE_EXTENSION_KEY), extnDesc.Key);
                extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
                extnDesc.Type    = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
                extnDesc.Data[0] = BayerFourCC2FormatFlag(pDst->Info.FourCC);
                hRes = SetResourceExtension(m_pD11Device, &extnDesc);
                MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_MEMORY_ALLOC);
            }
            ID3D11Texture2D *pStaging;
            hRes = m_pD11Device->CreateTexture2D(&desc, NULL, &pStaging);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_MEMORY_ALLOC);
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Dx11 Core Fast Copy ippiCopy");

            do
            {
                hRes = m_pD11Context->Map(pStaging, 0, D3D11_MAP_WRITE, D3D11_MAP_FLAG_DO_NOT_WAIT, &sLockRect);
            }
            while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);

            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
            dstPitch = sLockRect.RowPitch;

            switch (pDst->Info.FourCC)
            {
            case MFX_FOURCC_R16_BGGR:
            case MFX_FOURCC_R16_RGGB:
            case MFX_FOURCC_R16_GRBG:
            case MFX_FOURCC_R16_GBRG:
                ippiCopy_16u_C1R(pSrc->Data.Y16, srcPitch, (mfxU16 *)sLockRect.pData, dstPitch, roi);
                break;
            default:
                {
                    sts = mfxDefaultAllocatorD3D11::SetFrameData(sSurfDesc, sLockRect, pDst->Data);
                    MFX_CHECK_STS(sts);

                    mfxMemId saveMemId = pDst->Data.MemId;
                    pDst->Data.MemId = 0;

                    if (pDst->Info.FourCC == DXGI_FORMAT_AYUV)
                        pDst->Info.FourCC = MFX_FOURCC_AYUV;

                    if (pSrc->Info.FourCC == MFX_FOURCC_YV12)
                    {
                        sts = ConvertYV12toNV12SW(pDst, pSrc);
                        MFX_CHECK_STS(sts);
                    }

                    sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_VIDEO); // sw copy
                    MFX_CHECK_STS(sts);

                    pDst->Data.MemId = saveMemId;
                    MFX_CHECK_STS(sts);
                }
                break;
            }

            size_t index = (size_t)((mfxHDLPair*)pDst->Data.MemId)->second;

            m_pD11Context->Unmap(pStaging, 0);
            m_pD11Context->CopySubresourceRegion(pSurface, (mfxU32)index, 0, 0, 0, pStaging, 0, NULL);

            SAFE_RELEASE(pStaging);
        }
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return sts;
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::SetHandle(mfxHandleType type, mfxHDL handle)
{
    try
    {
        switch (type)
        {
        case MFX_HANDLE_D3D11_DEVICE:
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            // SetHandle should be first since 1.6 version
            bool isRequeredEarlySetHandle = (m_session->m_version.Major > 1 ||
                (m_session->m_version.Major == 1 && m_session->m_version.Minor >= 6));

            MFX_CHECK(!isRequeredEarlySetHandle || !m_pD11Device, MFX_ERR_UNDEFINED_BEHAVIOR);

            MFX_CHECK_HDL(handle);

            // If device manager already set, return error
            MFX_CHECK(!m_hdl, MFX_ERR_UNDEFINED_BEHAVIOR);

            CComPtr<ID3D11Device> tmp_device = reinterpret_cast<ID3D11Device*>(handle);

            CComPtr<ID3D11DeviceContext> pImmediateContext;
            tmp_device->GetImmediateContext(&pImmediateContext);

            MFX_CHECK(pImmediateContext, MFX_ERR_UNDEFINED_BEHAVIOR);

            CComQIPtr<ID3D10Multithread> p_mt(pImmediateContext);
            MFX_CHECK(p_mt && p_mt->GetMultithreadProtected(), MFX_ERR_UNDEFINED_BEHAVIOR);

            m_hdl = handle;

            m_pD11Device  = tmp_device;
            m_pD11Context = pImmediateContext;

            m_pD11VideoDevice  = m_pD11Device;
            m_pD11VideoContext = m_pD11Context;

            m_bUseExtManager = true;

            return MFX_ERR_NONE;
        }
        default:
            return Base::SetHandle(type, handle);
        }
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
}

template <class Base>
mfxStatus D3D11VideoCORE_T<Base>::GetHandle(mfxHandleType type, mfxHDL *handle)
{
    MFX_CHECK_NULL_PTR1(handle);
    if (type == MFX_HANDLE_D3D11_DEVICE)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        // not existing handle yet
        MFX_CHECK(m_hdl, MFX_ERR_NOT_FOUND);

        *handle = m_hdl;
        return MFX_ERR_NONE;
    }

    return Base::GetHandle(type, handle);
}

template <class Base>
bool D3D11VideoCORE_T<Base>::IsCompatibleForOpaq()
{
    if (!m_bUseExtManager)
    {
        return m_session->m_pOperatorCore->HaveJoinedSessions() == false;
    }
    return true;
}

D3D11VideoCORE20::D3D11VideoCORE20(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session)
    : D3D11VideoCORE20_base(adapterNum, numThreadsAvailable, session)
{
    m_enabled20Interface = false;

#if !defined STRIP_EMBARGO
    int deviceId = MFX::GetDeviceId(adapterNum);
    auto itDev = std::find_if(std::begin(listLegalDevIDs), std::end(listLegalDevIDs)
        , [deviceId](mfx_device_item dev) { return dev.device_id == deviceId; });

    if (itDev != std::end(listLegalDevIDs))
    {
        switch (itDev->platform)
        {
        case MFX_HW_TGL_LP:
        case MFX_HW_RKL:
        case MFX_HW_ADL_S:
        case MFX_HW_ADL_P:
        case MFX_HW_DG1:
        case MFX_HW_DG2:
            // These platforms support VPL feature set
            m_enabled20Interface = true;
            break;
        default:
            m_enabled20Interface = false;
            break;
        }
    }
#endif

    if (m_enabled20Interface)
        m_frame_allocator_wrapper.allocator_hw.reset(new FlexibleFrameAllocatorHW_D3D11(nullptr, m_session));
}

D3D11VideoCORE20::~D3D11VideoCORE20()
{}

mfxStatus D3D11VideoCORE20::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool isNeedCopy)
{
    if (!m_enabled20Interface)
        return D3D11VideoCORE_T<CommonCORE20>::AllocFrames(request, response, isNeedCopy);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE20::AllocFrames");

    MFX_CHECK_NULL_PTR2(request, response);
    MFX_CHECK(!(request->Type & 0x0004), MFX_ERR_UNSUPPORTED); // 0x0004 means MFX_MEMTYPE_OPAQUE_FRAME

    UMC::AutomaticUMCMutex guard(m_guard);
    try
    {
        if (request->Info.FourCC == MFX_FOURCC_P8

            // use internal allocator for R16 since creating requires
            // Intel's internal resource extensions that are not
            // exposed for external application
            || IsBayerFormat(request->Info.FourCC))
        {
            request->Type |= MFX_MEMTYPE_INTERNAL_FRAME;
        }

        // Create Service - first call
        mfxStatus sts = InitializeDevice();
        MFX_CHECK_STS(sts);

        m_frame_allocator_wrapper.SetDevice(m_pD11Device);

        if (!m_bCmCopy && m_bCmCopyAllowed && isNeedCopy)
        {
            m_pCmCopy.reset(new CmCopyWrapper);

            if (!m_pCmCopy->GetCmDevice<ID3D11Device>(m_pD11Device))
            {
                //!!!! WA: CM restricts multiple CmDevice creation from different device managers.
                //if failed to create CM device, continue without CmCopy
                m_bCmCopy = false;
                m_bCmCopyAllowed = false;

                m_pCmCopy.reset();
                //return MFX_ERR_DEVICE_FAILED;
            }
            else
            {
                sts = m_pCmCopy->Initialize(GetHWType());
                MFX_CHECK_STS(sts);
                m_bCmCopy = true;
            }
        }
        else if (m_bCmCopy)
        {
            if (m_pCmCopy)
                m_pCmCopy->ReleaseCmSurfaces();
            else
                m_bCmCopy = false;
        }
        if (m_pCmCopy && !m_bCmCopySwap &&
                              (  request->Info.FourCC == MFX_FOURCC_BGR4
                              || request->Info.FourCC == MFX_FOURCC_RGB4
                              || request->Info.FourCC == MFX_FOURCC_ARGB16
                              || request->Info.FourCC == MFX_FOURCC_ARGB16
                              || request->Info.FourCC == MFX_FOURCC_P010))
        {
            sts = m_pCmCopy->InitializeSwapKernels(GetHWType());
            m_bCmCopySwap = true;
        }

        return m_frame_allocator_wrapper.Alloc(*request, *response);
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }
}

mfxStatus D3D11VideoCORE20::ReallocFrame(mfxFrameSurface1 *surf)
{
    if (!m_enabled20Interface)
        return D3D11VideoCORE_T<CommonCORE20>::ReallocFrame(surf);

    MFX_CHECK_NULL_PTR1(surf);

    return m_frame_allocator_wrapper.ReallocSurface(surf->Info, surf->Data.MemId);
}

mfxStatus D3D11VideoCORE20::DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType)
{
    if (!m_enabled20Interface)
        return D3D11VideoCORE_T<CommonCORE20>::DoFastCopyWrapper(pDst, dstMemType, pSrc, srcMemType);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE20::DoFastCopyWrapper");

    MFX_CHECK_NULL_PTR2(pSrc, pDst);

    // TODO: consider uncommenting underlying checks later after additional validation
    //MFX_CHECK(!pSrc->Data.MemType || MFX_MEMTYPE_BASE(pSrc->Data.MemType) == MFX_MEMTYPE_BASE(srcMemType), MFX_ERR_UNSUPPORTED);
    //MFX_CHECK(!pDst->Data.MemType || MFX_MEMTYPE_BASE(pDst->Data.MemType) == MFX_MEMTYPE_BASE(dstMemType), MFX_ERR_UNSUPPORTED);

    mfxFrameSurface1 srcTempSurface = *pSrc, dstTempSurface = *pDst;
    srcTempSurface.Data.MemType = srcMemType;
    dstTempSurface.Data.MemType = dstMemType;

    mfxFrameSurface1_scoped_lock src_surf_lock(&srcTempSurface, this), dst_surf_lock(&dstTempSurface, this);
    mfxHDLPair handle_pair_src, handle_pair_dst;

    mfxStatus sts;
    if (srcTempSurface.Data.MemType & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
    {
        clear_frame_data(srcTempSurface.Data);
        sts = SwitchMemidInSurface(srcTempSurface, handle_pair_src);
        MFX_CHECK_STS(sts);
    }
    else
    {
        sts = src_surf_lock.lock(MFX_MAP_READ, SurfaceLockType::LOCK_GENERAL);
        MFX_CHECK_STS(sts);
        srcTempSurface.Data.MemId = 0;
    }

    if (dstTempSurface.Data.MemType & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
    {
        clear_frame_data(dstTempSurface.Data);
        sts = SwitchMemidInSurface(dstTempSurface, handle_pair_dst);
        MFX_CHECK_STS(sts);
    }
    else
    {
        sts = dst_surf_lock.lock(MFX_MAP_WRITE, SurfaceLockType::LOCK_GENERAL);
        MFX_CHECK_STS(sts);
        dstTempSurface.Data.MemId = 0;
    }

    sts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface);
    MFX_CHECK_STS(sts);

    sts = src_surf_lock.unlock();
    MFX_CHECK_STS(sts);

    return dst_surf_lock.unlock();
}

class TextureScopedLock
{
public:
    TextureScopedLock(ID3D11Device& device, ID3D11DeviceContext& context)
        : device(device)
        , context(context)
    {}

    ~TextureScopedLock()
    {
        context.Unmap(texture, 0);
        if (copy_after_unmap)
            context.CopySubresourceRegion(dst_texture, dst_texture_index, 0, 0, 0, texture, 0, nullptr);
    }

    CComPtr<ID3D11Texture2D> texture;
    ID3D11Device&            device;
    ID3D11DeviceContext&     context;
    // Copy Back to
    bool                     copy_after_unmap  = false;
    ID3D11Texture2D *        dst_texture       = nullptr;
    mfxU32                   dst_texture_index = 0;
};

mfxStatus CreateAndLockStagingSurf(mfxFrameSurface1 & mfx_frame_surface, TextureScopedLock& texture_scoped_lock, mfxU32 flags)
{
    MFX_CHECK(flags & MFX_MAP_READ_WRITE, MFX_ERR_UNSUPPORTED);

    ID3D11Texture2D * input_texture = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)mfx_frame_surface.Data.MemId)->first);
    MFX_CHECK_HDL(input_texture);

    D3D11_TEXTURE2D_DESC surf_descr = {};
    input_texture->GetDesc(&surf_descr);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = surf_descr.Width;
    desc.Height           = surf_descr.Height;
    desc.MipLevels        = 1;
    desc.Format           = surf_descr.Format;
    desc.SampleDesc.Count = 1;
    desc.ArraySize        = 1;
    desc.Usage            = D3D11_USAGE_STAGING;
    desc.BindFlags        = 0;

    D3D11_MAP mapType = D3D11_MAP_READ_WRITE;
    if (flags & MFX_MAP_READ)
    {
        mapType = D3D11_MAP_READ;
        desc.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
    }

    if (flags & MFX_MAP_WRITE)
    {
        mapType = D3D11_MAP_WRITE;
        desc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
    }

    if ((flags & MFX_MAP_READ_WRITE) == MFX_MAP_READ_WRITE)
    {
        mapType = D3D11_MAP_READ_WRITE;
    }

    HRESULT hRes;

    if (DXGI_FORMAT_R16_TYPELESS == surf_descr.Format)
    {
        RESOURCE_EXTENSION extnDesc = {};

        static_assert (sizeof RESOURCE_EXTENSION_KEY <= sizeof extnDesc.Key, "sizeof RESOURCE_EXTENSION_KEY > sizeof extnDesc.Key");
        std::copy(std::begin(RESOURCE_EXTENSION_KEY), std::end(RESOURCE_EXTENSION_KEY), extnDesc.Key);

        extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
        extnDesc.Type               = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
        extnDesc.Data[0]            = BayerFourCC2FormatFlag(mfx_frame_surface.Info.FourCC);

        hRes = SetResourceExtension(&texture_scoped_lock.device, &extnDesc);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_MEMORY_ALLOC);
    }

    ID3D11Texture2D *pStaging;
    hRes = texture_scoped_lock.device.CreateTexture2D(&desc, nullptr, &pStaging);
    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_MEMORY_ALLOC);

    texture_scoped_lock.texture.Attach(pStaging);

    size_t index = (size_t)((mfxHDLPair*)mfx_frame_surface.Data.MemId)->second;

    if (flags & MFX_MAP_READ)
    {
        texture_scoped_lock.context.CopySubresourceRegion(pStaging, 0, 0, 0, 0, input_texture, (mfxU32)index, nullptr);

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Dx11 Core Fast Copy SSE");
    }

    D3D11_MAPPED_SUBRESOURCE sLockRect = {};
    do
    {
        hRes = texture_scoped_lock.context.Map(pStaging, 0, mapType, D3D11_MAP_FLAG_DO_NOT_WAIT, &sLockRect);
    } while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);

    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

    texture_scoped_lock.copy_after_unmap = flags & MFX_MAP_WRITE;

    if (texture_scoped_lock.copy_after_unmap)
    {
        texture_scoped_lock.dst_texture       = input_texture;
        texture_scoped_lock.dst_texture_index = mfxU32(index);
    }

    return mfxDefaultAllocatorD3D11::SetFrameData(desc, sLockRect, mfx_frame_surface.Data);
}

mfxStatus D3D11VideoCORE20::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    if (!m_enabled20Interface)
        return D3D11VideoCORE_T<CommonCORE20>::DoFastCopyExtended(pDst, pSrc);

    MFX_CHECK_NULL_PTR2(pDst, pSrc);

    mfxU8 *srcPtr, *dstPtr;

    mfxStatus sts = GetFramePointerChecked(pSrc->Info, pSrc->Data, &srcPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    sts = GetFramePointerChecked(pDst->Info, pDst->Data, &dstPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    // check that only memId or pointer are passed
    // otherwise don't know which type of memory copying is requested
    MFX_CHECK(!!dstPtr != !!pDst->Data.MemId, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(!!srcPtr != !!pSrc->Data.MemId, MFX_ERR_UNDEFINED_BEHAVIOR);

    IppiSize roi = { std::min(pSrc->Info.Width, pDst->Info.Width), std::min(pSrc->Info.Height, pDst->Info.Height) };

    // check that region of interest is valid
    MFX_CHECK(roi.width && roi.height, MFX_ERR_UNDEFINED_BEHAVIOR);

    bool canUseCMCopy = m_bCmCopy && CmCopyWrapper::CanUseCmCopy(pDst, pSrc);

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if (canUseCMCopy)
        {
            return m_pCmCopy->CopyVideoToVideo(pDst, pSrc);
        }

        ID3D11Texture2D * pSurfaceSrc = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pSrc->Data.MemId)->first);
        ID3D11Texture2D * pSurfaceDst = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pDst->Data.MemId)->first);

        size_t indexSrc = (size_t)((mfxHDLPair*)pSrc->Data.MemId)->second;
        size_t indexDst = (size_t)((mfxHDLPair*)pDst->Data.MemId)->second;

        m_pD11Context->CopySubresourceRegion(pSurfaceDst, (mfxU32)indexDst, 0, 0, 0, pSurfaceSrc, (mfxU32)indexSrc, nullptr);

        return MFX_ERR_NONE;
    }

    if (NULL != pSrc->Data.MemId && NULL != dstPtr)
    {
        if (canUseCMCopy)
        {
            return m_pCmCopy->CopyVideoToSys(pDst, pSrc);
        }

        TextureScopedLock texture_lock(*m_pD11Device, *m_pD11Context);
        sts = CreateAndLockStagingSurf(*pSrc, texture_lock, MFX_MAP_READ);
        MFX_CHECK_STS(sts);

        mfxMemId saveMemId = pSrc->Data.MemId;
        pSrc->Data.MemId = 0;

        if (pDst->Info.FourCC == DXGI_FORMAT_AYUV)
            pDst->Info.FourCC = MFX_FOURCC_AYUV;

        sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_VIDEO_TO_SYS); // sw copy
        MFX_CHECK_STS(sts);

        pSrc->Data.MemId = saveMemId;

        return MFX_ERR_NONE;
    }

    if (NULL != srcPtr && NULL != dstPtr)
    {
        // system memories were passed
        // use common way to copy frames

        if (pDst->Info.FourCC == DXGI_FORMAT_AYUV)
            pDst->Info.FourCC = MFX_FOURCC_AYUV;

        return CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_SYS); // sw copy
    }

    if (NULL != srcPtr && NULL != pDst->Data.MemId)
    {
        // TODO: support of Bayer formats in CopySysToVideo
        /*
        // source are placed in system memory, destination is in video memory
        // use common way to copy frames from system to video, most faster
        mfxI64 verticalPitch = (mfxI64)(pSrc->Data.UV - pSrc->Data.Y);
        verticalPitch = (verticalPitch % pSrc->Data.Pitch) ? 0 : verticalPitch / pSrc->Data.Pitch;
        if ( IsBayerFormat(pSrc->Info.FourCC) )
        {
            // Only one plane is used for Bayer and vertical pitch calculation is not correct for it.
            verticalPitch = pDst->Info.Height;
        }*/

        if (canUseCMCopy)
        {
            return m_pCmCopy->CopySysToVideo(pDst, pSrc);
        }

        TextureScopedLock texture_lock(*m_pD11Device, *m_pD11Context);
        sts = CreateAndLockStagingSurf(*pDst, texture_lock, MFX_MAP_WRITE);
        MFX_CHECK_STS(sts);

        switch (pDst->Info.FourCC)
        {
        case MFX_FOURCC_R16_BGGR:
        case MFX_FOURCC_R16_RGGB:
        case MFX_FOURCC_R16_GRBG:
        case MFX_FOURCC_R16_GBRG:
        {
            mfxU32 srcPitch = pSrc->Data.PitchLow + ((mfxU32)pSrc->Data.PitchHigh << 16);
            mfxU32 dstPitch = pDst->Data.PitchLow + ((mfxU32)pDst->Data.PitchHigh << 16);

            IppStatus ippSts = ippiCopy_16u_C1R(pSrc->Data.Y16, srcPitch, pDst->Data.Y16, dstPitch, roi);
            MFX_CHECK(ippSts == ippStsNoErr, MFX_ERR_UNKNOWN);
        }
        break;
        default:
        {
            mfxMemId saveMemId = pDst->Data.MemId;
            pDst->Data.MemId = 0;

            if (pDst->Info.FourCC == DXGI_FORMAT_AYUV)
                pDst->Info.FourCC = MFX_FOURCC_AYUV;

            sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_VIDEO); // sw copy
            MFX_CHECK_STS(sts);

            pDst->Data.MemId = saveMemId;
        }
        break;
        }

        return MFX_ERR_NONE;
    }

    MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
}

mfxStatus D3D11VideoCORE20::CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* & surf)
{
    MFX_CHECK(m_enabled20Interface, MFX_ERR_UNSUPPORTED);

    MFX_SAFE_CALL(InitializeDevice());
    m_frame_allocator_wrapper.SetDevice(m_pD11Device);

    return m_frame_allocator_wrapper.CreateSurface(type, info, surf);
}


template class D3D11VideoCORE_T<CommonCORE  >;
template class D3D11VideoCORE_T<CommonCORE20>;

#endif
