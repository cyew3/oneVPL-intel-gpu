/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

File Name: libmf_core_hw.cpp

\* ****************************************************************************** */

#include "mfx_common.h"

#include <atlbase.h>
#if defined (MFX_VA_WIN) && defined (MFX_D3D9_ENABLED)

#include "umc_va_dxva2.h"
#include "libmfx_core_d3d9.h"
#include "mfx_utils.h"
#include "mfx_session.h"
#include "mfx_check_hardware_support.h"
#include "ippi.h"

#include "umc_va_dxva2_protected.h"
#include "mfx_common_decode_int.h"
#include "mfx_enc_common.h"
#include "libmfx_core_hw.h"

#include "cm_mem_copy.h"

#include "mfx_session.h"
#define MFX_CHECK_HDL(hdl) {if (!hdl) MFX_RETURN(MFX_ERR_INVALID_HANDLE);}
//#define MFX_GUID_CHECKING
using namespace std;
using namespace UMC;

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report,
0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);

mfxStatus CreateD3DDevice(IDirect3D9        **pD3D,
                          IDirect3DDevice9  **pDirect3DDevice,
                          const mfxU32      adapterNum,
                          mfxU16            width,
                          mfxU16            height)
{
    D3DPRESENT_PARAMETERS d3dpp;
    memset(&d3dpp, 0, sizeof(D3DPRESENT_PARAMETERS));

    width; height;

    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.BackBufferCount = 0;

    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = 0;
    d3dpp.Windowed = TRUE;
    d3dpp.Flags = D3DPRESENTFLAG_VIDEO;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hres;
    {
        IDirect3D9Ex *pD3DEx = NULL;
        IDirect3DDevice9Ex  *pDirect3DDeviceEx = NULL;

        Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3DEx);
        if(NULL == pD3DEx)
            return MFX_ERR_DEVICE_FAILED;

        // let try to create DeviceEx ahead of regular Device
        // for details - see MSDN
        hres = pD3DEx->CreateDeviceEx(
            adapterNum,
            D3DDEVTYPE_HAL,
            0,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED,
            &d3dpp,
            NULL,
            &pDirect3DDeviceEx);

        if (S_OK!=hres)
        {
            pD3DEx->Release();

            //1 create DX device
            *pD3D = Direct3DCreate9(D3D_SDK_VERSION);
            if (0 == *pD3D)
                return MFX_ERR_DEVICE_FAILED;

            // let try to create regulare device
            hres = (*pD3D)->CreateDevice(
                adapterNum,
                D3DDEVTYPE_HAL,
                0,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED,
                &d3dpp,
                pDirect3DDevice);

            if (S_OK!=hres)
            {
                (*pD3D)->Release();
                return MFX_ERR_DEVICE_FAILED;
            }
            else
                return MFX_ERR_NONE;
        }
        else
        {
            *pD3D = pD3DEx;
            *pDirect3DDevice = pDirect3DDeviceEx;
            return MFX_ERR_NONE;
        }

    }
}

mfxStatus D3D9VideoCORE::GetIntelDataPrivateReport(const GUID guid, DXVA2_ConfigPictureDecode & config)
{
    if (!m_pDirectXVideoService)
    {
        mfxStatus sts = InitializeService(true);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    mfxU32 cDecoderGuids = 0;
    GUID   *pDecoderGuids = 0;
    HRESULT hr = m_pDirectXVideoService->GetDecoderDeviceGuids(&cDecoderGuids, &pDecoderGuids);
    if (FAILED(hr))
        return MFX_WRN_PARTIAL_ACCELERATION;

    bool isRequestedGuidPresent = false;
    bool isIntelGuidPresent = false;

    for (mfxU32 i = 0; i < cDecoderGuids; i++)
    {
        if (guid == pDecoderGuids[i])
            isRequestedGuidPresent = true;

        if (DXVADDI_Intel_Decode_PrivateData_Report == pDecoderGuids[i])
            isIntelGuidPresent = true;
    }

    if (pDecoderGuids)
    {
        CoTaskMemFree(pDecoderGuids);
    }

    if (!isRequestedGuidPresent)
        //return MFX_ERR_NOT_FOUND;

    if (!isIntelGuidPresent) // if no required GUID - no acceleration at all
        return MFX_WRN_PARTIAL_ACCELERATION;

    DXVA2_ConfigPictureDecode *pConfig = 0;
    UINT cConfigurations = 0;
    DXVA2_VideoDesc VideoDesc = {0};

    hr = m_pDirectXVideoService->GetDecoderConfigurations(DXVADDI_Intel_Decode_PrivateData_Report,
                                            &VideoDesc,
                                            NULL,
                                            &cConfigurations,
                                            &pConfig);
    if (FAILED(hr) || pConfig == 0)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }
    for (mfxU32 k = 0; k < cConfigurations; k++)
    {
        if (pConfig[k].guidConfigBitstreamEncryption == guid)
        {
            memcpy_s(&config, sizeof(config), &pConfig[k], sizeof(DXVA2_ConfigPictureDecode));
            if (pConfig)
            {
                CoTaskMemFree(pConfig);
            }
            if (guid == DXVA2_Intel_Encode_AVC && config.ConfigSpatialResid8 != AVC_D3D9_DDI_VERSION)
            {
                return MFX_WRN_PARTIAL_ACCELERATION;
            }
            else if (guid == DXVA2_Intel_Encode_MPEG2 && config.ConfigSpatialResid8 != INTEL_MPEG2_ENCODE_DDI_VERSION)
            {
                return  MFX_WRN_PARTIAL_ACCELERATION;
            }
            else if (guid == DXVA2_Intel_Encode_JPEG  && config.ConfigSpatialResid8 != INTEL_MJPEG_ENCODE_DDI_VERSION)
            {
                return  MFX_WRN_PARTIAL_ACCELERATION;
            }/*
            else if (guid == DXVA2_Intel_Encode_HEVC_Main)
            {
                m_HEVCEncodeDDIVersion = video_config.ConfigSpatialResid8;
            }*/
            else
                 return  MFX_ERR_NONE;
        }
    }

    if (pConfig)
    {
        CoTaskMemFree(pConfig);
    }

    return MFX_WRN_PARTIAL_ACCELERATION;
}

mfxStatus D3D9VideoCORE::IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncoder)
{
    if (!par)
        return MFX_WRN_PARTIAL_ACCELERATION;
    mfxStatus sts = MFX_ERR_NONE;
    if (!m_pDirectXVideoService)
    {
        mfxStatus sts = InternalInit();
        if (sts < MFX_ERR_NONE)
            return MFX_WRN_PARTIAL_ACCELERATION;

        sts = InitializeService(true);
        if (sts < MFX_ERR_NONE)
            return MFX_WRN_PARTIAL_ACCELERATION;

        AuxiliaryDevice                      auxDevice;
        // to be sure about SG case need to check Intel Auxialary device
        sts = auxDevice.Initialize(0, m_pDirectXVideoService);
        if (MFX_ERR_NONE != sts)
            return MFX_WRN_PARTIAL_ACCELERATION;

        if (isEncoder)
        {
            sts = auxDevice.IsAccelerationServiceExist(guid);
        }
    }
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;
    DXVA2_ConfigPictureDecode config;
    sts = GetIntelDataPrivateReport(guid, config);

    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    if (sts == MFX_ERR_NONE && !isEncoder)
    {
        return CheckIntelDataPrivateReport<DXVA2_ConfigPictureDecode>(&config, par);
    }
    else
        return sts;

}

D3D9VideoCORE::D3D9VideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session)
: CommonCORE(numThreadsAvailable, session)
, m_hDirectXHandle(INVALID_HANDLE_VALUE)
, m_pDirect3DDeviceManager(0)
, m_pDirect3DDevice(0)
, m_pDirectXVideoService(0)
, m_bUseExtAllocForHWFrames(false)
, m_pD3D(NULL)
, m_adapterNum(adapterNum)
, m_pSystemMemorySurface(0)
, m_HWType(MFX_HW_UNKNOWN)
, m_bCmCopy(false)
, m_bCmCopySwap(false)
, m_bCmCopyAllowed(true)
{
    m_pAdapter.reset(new D3D9Adapter(this));
}

eMFXHWType D3D9VideoCORE::GetHWType()
{
    InternalInit();
    return m_HWType;
}

mfxStatus D3D9VideoCORE::InternalInit()
{
    if (m_HWType != MFX_HW_UNKNOWN)
        return MFX_ERR_NONE;

    mfxU32 platformFromDriver = 0;

    DXVA2_ConfigPictureDecode config;
    mfxStatus sts = GetIntelDataPrivateReport(sDXVA2_ModeH264_VLD_NoFGT, config);
    if (sts == MFX_ERR_NONE)
        platformFromDriver = config.ConfigBitstreamRaw;

    m_HWType = MFX::GetHardwareType(m_adapterNum, platformFromDriver);

    if (m_HWType == MFX_HW_LAKE)
    {
        OnDeblockingInWinRegistry(MFX_CODEC_AVC);
        OnDeblockingInWinRegistry(MFX_CODEC_VC1);
    }

    //if (m_HWType >= MFX_HW_BDW)
    //    m_bCmCopyAllowed = false;

    if (platformFromDriver == 12) // 12 - IGFX_GT, sandybridge
        m_bCmCopyAllowed = false;

    return MFX_ERR_NONE;
}

D3D9VideoCORE::~D3D9VideoCORE()
{
    if (m_bCmCopy)
    {
        m_pCmCopy.get()->Release();
        m_bCmCopy = false;
    }

    Close();
}

void D3D9VideoCORE::Close()
{
    m_pVA.reset();
    // Should be enabled after merge from SNB branch
    //if (m_pFastComposing.get())
    //    m_pFastComposing.get()->Release();
    SAFE_RELEASE(m_pDirectXVideoService);
    SAFE_RELEASE(m_pDirect3DDevice);
    SAFE_RELEASE(m_pD3D);
    if (m_pDirect3DDeviceManager && m_hDirectXHandle != INVALID_HANDLE_VALUE)
    {
        m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
        m_hDirectXHandle = INVALID_HANDLE_VALUE;
    }

    if (!m_bUseExtManager || m_pDirect3DDeviceManager != m_hdl)
    {
        SAFE_RELEASE(m_pDirect3DDeviceManager);
        SAFE_RELEASE(m_pSystemMemorySurface);
    }
}

mfxStatus D3D9VideoCORE::SetHandle(mfxHandleType type, mfxHDL hdl)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    try
    {
        // SetHandle should be first since 1.6 version
        bool isRequeredEarlySetHandle = (m_session->m_version.Major > 1 ||
            (m_session->m_version.Major == 1 && m_session->m_version.Minor >= 6));

        if ((type == MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 || type == MFX_HANDLE_D3D11_DEVICE) && isRequeredEarlySetHandle && m_pDirect3DDeviceManager)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        mfxStatus sts = CommonCORE::SetHandle(type, hdl);
        MFX_CHECK_STS(sts);

#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
        m_vpp_hw_resmng.Close();
#endif

        if (m_pDirect3DDeviceManager)
            Close();

        m_pDirect3DDeviceManager = (IDirect3DDeviceManager9 *)m_hdl;
        HANDLE DirectXHandle;
        IDirect3DDevice9* pD3DDevice;
        D3DDEVICE_CREATION_PARAMETERS pParameters;
        HRESULT hr = m_pDirect3DDeviceManager->OpenDeviceHandle(&DirectXHandle);
        if (FAILED(hr))
        {
            ReleaseHandle();
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        hr = m_pDirect3DDeviceManager->LockDevice(DirectXHandle, &pD3DDevice, true);
        if (FAILED(hr))
        {
            m_pDirect3DDeviceManager->CloseDeviceHandle(DirectXHandle);
            ReleaseHandle();
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        hr = pD3DDevice->GetCreationParameters(&pParameters);
        if (FAILED(hr))
        {
            m_pDirect3DDeviceManager->UnlockDevice(DirectXHandle, true);
            m_pDirect3DDeviceManager->CloseDeviceHandle(DirectXHandle);
            pD3DDevice->Release();
            ReleaseHandle();
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        if (!(pParameters.BehaviorFlags & D3DCREATE_MULTITHREADED))
        {
            m_pDirect3DDeviceManager->UnlockDevice(DirectXHandle, true);
            m_pDirect3DDeviceManager->CloseDeviceHandle(DirectXHandle);
            pD3DDevice->Release();
            ReleaseHandle();
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        m_pDirect3DDeviceManager->UnlockDevice(DirectXHandle, true);
        m_pDirect3DDeviceManager->CloseDeviceHandle(DirectXHandle);
        pD3DDevice->Release();
        return MFX_ERR_NONE;
    }
    catch (MFX_CORE_CATCH_TYPE)
    {
        ReleaseHandle();
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
}// mfxStatus D3D9VideoCORE::SetHandle(mfxHandleType type, mfxHDL handle)


mfxStatus D3D9VideoCORE::AllocFrames(mfxFrameAllocRequest *request,
                                   mfxFrameAllocResponse *response, bool isNeedCopy)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    try
    {
        MFX_CHECK_NULL_PTR2(request, response);
        mfxStatus sts = MFX_ERR_NONE;
        mfxFrameAllocRequest temp_request = *request;

        // external allocator doesn't know how to allocate opaque surfaces
        // we can treat opaque as internal
        if (temp_request.Type & MFX_MEMTYPE_OPAQUE_FRAME)
        {
            temp_request.Type -= MFX_MEMTYPE_OPAQUE_FRAME;
            temp_request.Type |= MFX_MEMTYPE_INTERNAL_FRAME;
        }

        if (!m_bFastCopy)
        {
            // initialize fast copy
            m_pFastCopy.reset(new FastCopy());
            m_pFastCopy.get()->Initialize();

            m_bFastCopy = true;
        }

        // Create Service - first call
        sts = GetD3DService(request->Info.Width, request->Info.Height);
        MFX_CHECK_STS(sts);

        if (!m_bCmCopy && m_bCmCopyAllowed && isNeedCopy)
        {
            m_pCmCopy.reset(new CmCopyWrapper);
            if (!m_pCmCopy.get()->GetCmDevice<IDirect3DDeviceManager9>(m_pDirect3DDeviceManager)){
                //!!!! WA: CM restricts multiple CmDevice creation from different device managers.
                //if failed to create CM device, continue without CmCopy
                m_bCmCopy = false;
                m_bCmCopyAllowed = false;
                m_pCmCopy.get()->Release();
                m_pCmCopy.reset();
                //return MFX_ERR_DEVICE_FAILED;
            }else{
                sts = m_pCmCopy.get()->Initialize(GetHWType());
                MFX_CHECK_STS(sts);
                m_bCmCopy = true;
            }
        }else if(m_bCmCopy){
            if(m_pCmCopy.get())
                m_pCmCopy.get()->ReleaseCmSurfaces();
            else
                m_bCmCopy = false;
        }
        
        if(m_pCmCopy.get() && !m_bCmCopySwap && (request->Info.FourCC == MFX_FOURCC_BGR4 || request->Info.FourCC == MFX_FOURCC_RGB4 || request->Info.FourCC == MFX_FOURCC_ARGB16|| request->Info.FourCC == MFX_FOURCC_ABGR16 || request->Info.FourCC == MFX_FOURCC_P010))
        {
            sts = m_pCmCopy.get()->InitializeSwapKernels(GetHWType());
            m_bCmCopySwap = true;
        }

        // use common core for sw surface allocation
        if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            return CommonCORE::AllocFrames(request, response);
        else
        {
            // external allocator
            if (m_bSetExtFrameAlloc)
            {
                // Default allocator should be used if D3D manager is not set and internal D3D buffers are required
                if (!m_pDirect3DDeviceManager && request->Type & MFX_MEMTYPE_INTERNAL_FRAME)
                {
                    m_bUseExtAllocForHWFrames = false;
                    sts = DefaultAllocFrames(request, response);
                    MFX_CHECK_STS(sts);

                    return sts;
                }

                sts = (*m_FrameAllocator.frameAllocator.Alloc)(m_FrameAllocator.frameAllocator.pthis, &temp_request, response);

                // if external allocator cannot allocate d3d frames - use default memory allocator
                if (MFX_ERR_UNSUPPORTED == sts || MFX_ERR_MEMORY_ALLOC == sts)
                {
                    // Default Allocator is used for internal memory allocation only
                    if (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME)
                        return sts;
                    m_bUseExtAllocForHWFrames = false;
                    sts = DefaultAllocFrames(request, response);
                    MFX_CHECK_STS(sts);

                    return sts;
                }
                // let's create video accelerator
                else if (MFX_ERR_NONE == sts)
                {
                    // Checking for unsupported mode - external allocator exist but Device handle doesn't set
                    if (!m_pDirect3DDeviceManager)
                        return MFX_ERR_UNSUPPORTED;

                    m_bUseExtAllocForHWFrames = true;
                    sts = ProcessRenderTargets(request, response, &m_FrameAllocator);
                    if (response->NumFrameActual < request->NumFrameMin)
                    {
                        (*m_FrameAllocator.frameAllocator.Free)(m_FrameAllocator.frameAllocator.pthis, response);
                        return MFX_ERR_MEMORY_ALLOC;
                    }
                    MFX_CHECK_STS(sts);

                    return sts;
                }
                // error situation
                else
                {
                    m_bUseExtAllocForHWFrames = false;
                    return sts;
                }
            }
            else
            {
                // Default Allocator is used for internal memory allocation only
                if (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME)
                    return MFX_ERR_MEMORY_ALLOC;

                m_bUseExtAllocForHWFrames = false;
                sts = DefaultAllocFrames(request, response);
                MFX_CHECK_STS(sts);

                return sts;
            }
        }
    }
    catch(MFX_CORE_CATCH_TYPE)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
}

mfxStatus D3D9VideoCORE::DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts = MFX_ERR_NONE;

    if ((request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET)||
        (request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) // SW - TBD !!!!!!!!!!!!!!
    {
        // Create Service - first call
        sts = GetD3DService(request->Info.Width, request->Info.Height);
        MFX_CHECK_STS(sts);

        mfxBaseWideFrameAllocator* pAlloc = GetAllocatorByReq(request->Type);
        // VPP, ENC, PAK can request frames for several times
        if (pAlloc && (request->Type & MFX_MEMTYPE_FROM_DECODE))
            return MFX_ERR_MEMORY_ALLOC;

        if (!pAlloc)
        {
            m_pcHWAlloc.reset(new mfxDefaultAllocatorD3D9::mfxWideHWFrameAllocator(request->Type, m_pDirectXVideoService));
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
        return CommonCORE::DefaultAllocFrames(request, response);
    }
    ++m_NumAllocators;
    return sts;
}

mfxStatus D3D9VideoCORE::CreateVA(mfxVideoParam * param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, UMC::FrameAllocator *allocator)
{
    if(!param || !request || !response)
        return MFX_ERR_NULL_PTR;
    if (!(request->Type & MFX_MEMTYPE_FROM_DECODE) ||
        !(request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET))
        return MFX_ERR_NONE;

    std::vector<IDirect3DSurface9*>  renderTargets(response->NumFrameActual);

    for (mfxU32 i = 0; i < response->NumFrameActual; i++)
    {
        mfxMemId InternalMid = response->mids[i];
        mfxFrameAllocator* pAlloc = GetAllocatorAndMid(InternalMid);
        if (pAlloc)
            pAlloc->GetHDL(pAlloc->pthis, InternalMid, (mfxHDL*)(&renderTargets[i]));
        else
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return CreateVideoAccelerator(param, response->NumFrameActual, &renderTargets[0], allocator);
}

mfxStatus D3D9VideoCORE::CreateVideoProcessing(mfxVideoParam * param)
{
    mfxStatus sts = MFX_ERR_NONE;
    #if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
    if (!m_vpp_hw_resmng.GetDevice()){
        sts = m_vpp_hw_resmng.CreateDevice(this);
    }
    #else
    param;
    sts = MFX_ERR_UNSUPPORTED;
    #endif
    return sts;
}

mfxStatus D3D9VideoCORE::ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc)
{
    mfxStatus sts;
    sts = RegisterMids(response, request->Type, !m_bUseExtAllocForHWFrames, pAlloc);
    MFX_CHECK_STS(sts);
    m_pcHWAlloc.pop();
    return MFX_ERR_NONE;
}

mfxStatus D3D9VideoCORE::InitializeService(bool isTemporalCall)
{
    HRESULT hr = S_OK;

    if (!m_hdl && m_pDirect3DDeviceManager && !isTemporalCall)
    {
        m_hdl = (mfxHDL)m_pDirect3DDeviceManager;
    }

    // check if created already
    if (m_pDirectXVideoService)
    {
        return MFX_ERR_NONE;
    }

    if (!m_pDirect3DDeviceManager)
    {
        if (!m_pDirect3DDevice)
        {
            MFX_CHECK_STS(CreateD3DDevice(&m_pD3D, &m_pDirect3DDevice, m_adapterNum, 0, 0));
        }

        UINT resetToken;
        hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &m_pDirect3DDeviceManager);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        if (!isTemporalCall)
            m_hdl = (mfxHDL)m_pDirect3DDeviceManager;

        //set device to manager
        hr = m_pDirect3DDeviceManager->ResetDevice(m_pDirect3DDevice, resetToken);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    hr = m_pDirect3DDeviceManager->OpenDeviceHandle(&m_hDirectXHandle);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    hr = m_pDirect3DDeviceManager->GetVideoService(m_hDirectXHandle,
                                                   __uuidof(IDirectXVideoDecoderService),
                                                   (void**)&m_pDirectXVideoService);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

mfxStatus D3D9VideoCORE::GetD3DService(mfxU16 , mfxU16 , IDirectXVideoDecoderService **ppVideoService, bool isTemporal)
{
    mfxStatus sts = InternalInit();
    if (sts != MFX_ERR_NONE)
        return sts;

    sts = D3D9VideoCORE::InitializeService(isTemporal);
    if (sts != MFX_ERR_NONE)
        return sts;

    if (ppVideoService)
        *ppVideoService = m_pDirectXVideoService;

    return MFX_ERR_NONE;
}

mfxStatus D3D9VideoCORE::CreateVideoAccelerator(mfxVideoParam * param, int NumOfRenderTarget, IDirect3DSurface9 **RenderTargets, UMC::FrameAllocator *allocator)
{
    mfxStatus sts = MFX_ERR_NONE;
    m_pVA.reset(new DXVA2Accelerator);

    m_pVA->m_HWPlatform = ConvertMFXToUMCType(m_HWType);

    mfxFrameInfo *pInfo = &(param->mfx.FrameInfo);
    sts = GetD3DService(pInfo->Width, pInfo->Height);
    MFX_CHECK_STS(sts);

    m_pVA->SetDeviceManager(m_pDirect3DDeviceManager);

    UMC::VideoAcceleratorParams params;

    params.m_protectedVA = param->Protected;

    mfxU32 profile = ChooseProfile(param, GetHWType());

    UMC::VideoStreamInfo VideoInfo;
    ConvertMFXParamsToUMC(param, &VideoInfo);

    m_pVA->m_Platform = UMC::VA_DXVA2;
    m_pVA->m_Profile = (VideoAccelerationProfile)profile;

    // Init Accelerator
    params.m_pVideoStreamInfo = &VideoInfo;
    params.m_iNumberSurfaces = NumOfRenderTarget;
    params.m_surf = (void **)RenderTargets;
    params.m_allocator = allocator;

    if (UMC_OK != m_pVA->Init(&params))
    {
        m_pVA.reset();
        return MFX_ERR_UNSUPPORTED;
    }

    if (IS_PROTECTION_ANY(param->Protected) && !IS_PROTECTION_WIDEVINE(param->Protected))
    {
        DXVA2_DecodeExtensionData DecodeExtension;
        DecodeExtension.Function = DXVA2_DECODE_GET_DRIVER_HANDLE;
        DecodeExtension.pPrivateInputData = NULL;
        DecodeExtension.PrivateInputDataSize = 0;
        DecodeExtension.pPrivateOutputData = &m_DXVA2DecodeHandle;
        DecodeExtension.PrivateOutputDataSize = sizeof(m_DXVA2DecodeHandle);

        if (UMC_OK != m_pVA->ExecuteExtensionBuffer(&DecodeExtension))
        {
            m_pVA.reset();
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else
        m_DXVA2DecodeHandle = NULL;

    m_pVA->GetVideoDecoder(&m_D3DDecodeHandle);

    return sts;
}

#define REGSUBPATH TEXT("Software\\Intel\\Display\\DXVA")
#define AVCILDBMODE TEXT("AVC ILDB Mode")
#define VC1ILDBMODE TEXT("VC1 ILDB Mode")

mfxStatus D3D9VideoCORE::OnDeblockingInWinRegistry(mfxU32 codecId)
{
    if (GetPlatformType() == MFX_PLATFORM_SOFTWARE)
        return MFX_ERR_UNSUPPORTED;

    // handle to an open registry key
    HKEY hKey;

    // create the registry key. if the key already exists, the function opens it
    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, REGSUBPATH, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
        &hKey, NULL))
    {
        // not enough permissions open or create registry key! deblocking is disabled
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    LONG sts = ERROR_SUCCESS;
    // value for the deblocking key. 1 is on deblocking
    DWORD dwValue = 1;

    switch (codecId)
    {
    case MFX_CODEC_VC1:
        // sets or creates VC1 ILDB value under the registry key
        sts = RegSetValueEx(hKey, VC1ILDBMODE, 0, REG_DWORD,
            (LPBYTE) &dwValue, sizeof(DWORD));
        break;

    case MFX_CODEC_AVC:
        // sets or creates ILDB value under the registry key
        sts = RegSetValueEx(hKey, AVCILDBMODE, 0, REG_DWORD,
            (LPBYTE) &dwValue, sizeof(DWORD));
        break;

    default:
        // return unsupported error
        return MFX_ERR_UNSUPPORTED;
    }

    RegCloseKey(hKey);
    return ERROR_SUCCESS == sts ? MFX_ERR_NONE : MFX_ERR_UNDEFINED_BEHAVIOR;
}

mfxStatus D3D9VideoCORE::DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType)
{
    mfxStatus sts;

    mfxHDL srcHandle, dstHandle;
    mfxMemId srcMemId, dstMemId;

    mfxFrameSurface1 srcTempSurface, dstTempSurface;

    memset(&srcTempSurface, 0, sizeof(mfxFrameSurface1));
    memset(&dstTempSurface, 0, sizeof(mfxFrameSurface1));

    // save original mem ids
    srcMemId = pSrc->Data.MemId;
    dstMemId = pDst->Data.MemId;

    srcTempSurface.Info = pSrc->Info;
    dstTempSurface.Info = pDst->Info;

    bool isSrcLocked = false;
    bool isDstLocked = false;

    if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == pSrc->Data.Y)
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
            sts = GetExternalFrameHDL(srcMemId, &srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = srcHandle;
        }
    }
    else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == pSrc->Data.Y)
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
            sts = GetFrameHDL(srcMemId, &srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = srcHandle;
        }
    }

    if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == pDst->Data.Y)
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
            sts = GetExternalFrameHDL(dstMemId, &dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = dstHandle;
        }
    }
    else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == pDst->Data.Y)
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
            sts = GetFrameHDL(dstMemId, &dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = dstHandle;
        }
    }

    sts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface);
    MFX_CHECK_STS(sts);

    if (true == isSrcLocked)
    {
        if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = UnlockExternalFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(sts);
        }
        else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = UnlockFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(sts);
        }
    }

    if (true == isDstLocked)
    {
        if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = UnlockExternalFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(sts);
        }
        else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = UnlockFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D9VideoCORE::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    mfxStatus sts;

    // check that only memId or pointer are passed
    // otherwise don't know which type of memory copying is requested
    if (
        (NULL != pDst->Data.Y && NULL != pDst->Data.MemId) ||
        (NULL != pSrc->Data.Y && NULL != pSrc->Data.MemId)
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
    D3DSURFACE_DESC sSurfDesc;
    D3DLOCKED_RECT  sLockRect;

    mfxU32 srcPitch = pSrc->Data.PitchLow + ((mfxU32)pSrc->Data.PitchHigh << 16);
    mfxU32 dstPitch = pDst->Data.PitchLow + ((mfxU32)pDst->Data.PitchHigh << 16);

    FastCopy *pFastCopy = m_pFastCopy.get();
    if(!pFastCopy){
        m_pFastCopy.reset(new FastCopy());
        m_pFastCopy.get()->Initialize();
        m_bFastCopy = true;
        pFastCopy = m_pFastCopy.get();
    }
    CmCopyWrapper *pCmCopy = m_pCmCopy.get();

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if (m_bCmCopy == true && pDst->Info.FourCC != MFX_FOURCC_YV12 && CM_SUPPORTED_COPY_SIZE(roi))
        {
            mfxStatus sts = MFX_ERR_NONE;
            mfxU32 counter = 0;
            do
            {
                if(pSrc->Info.FourCC == MFX_FOURCC_RGB4 && pDst->Info.FourCC == MFX_FOURCC_BGR4)
                    sts = pCmCopy->CopySwapVideoToVideoMemory(pDst->Data.MemId, pSrc->Data.MemId, roi, MFX_FOURCC_BGR4);
                else
                    sts = pCmCopy->CopyVideoToVideoMemoryAPI(pDst->Data.MemId, pSrc->Data.MemId, roi);

                if (sts != MFX_ERR_NONE)
                    Sleep(20);
            }
            while(sts != MFX_ERR_NONE && ++counter < 4); // waiting 80 ms, source surface may be locked by application

            MFX_CHECK_STS(sts);
        }
        else
        {
            IDirect3DDevice9    *direct3DDevice;
            hRes = m_pDirect3DDeviceManager->LockDevice(m_hDirectXHandle, &direct3DDevice, true);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

            const tagRECT rect = {0, 0, roi.width, roi.height};

            HRESULT stretchRectResult = S_OK;
            mfxU32 counter = 0;
            do
            {
                stretchRectResult = direct3DDevice->StretchRect((IDirect3DSurface9*) pSrc->Data.MemId, &rect,
                                                                (IDirect3DSurface9*) pDst->Data.MemId, &rect,
                                                                 D3DTEXF_LINEAR);
                if (FAILED(stretchRectResult))
                    Sleep(20);
            }
            while(FAILED(stretchRectResult) && ++counter < 4); // waiting 80 ms, source surface may be locked by application

            hRes = m_pDirect3DDeviceManager->UnlockDevice(m_hDirectXHandle, false);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

            direct3DDevice->Release();

            MFX_CHECK(SUCCEEDED(stretchRectResult), MFX_ERR_DEVICE_FAILED);
        }
    }
    else if (NULL != pSrc->Data.MemId && NULL != pDst->Data.Y)
    {
        mfxI64 verticalPitch = (mfxI64)(pDst->Data.UV - pDst->Data.Y);
        verticalPitch = (verticalPitch % pDst->Data.Pitch)? 0 : verticalPitch / pDst->Data.Pitch;
        if (m_bCmCopy == true && CM_ALIGNED(pDst->Data.Pitch) && (pDst->Info.FourCC == MFX_FOURCC_NV12 || (pDst->Info.FourCC == MFX_FOURCC_P010 && pDst->Info.Shift == pSrc->Info.Shift)) && CM_ALIGNED(pDst->Data.Y) && CM_ALIGNED(pDst->Data.UV) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pDst->Info.Height && verticalPitch <= 16384)
        {
            if(m_HWType >= MFX_HW_SCL)
                sts = pCmCopy->CopyVideoToSystemMemory(pDst->Data.Y, pDst->Data.Pitch, (mfxU32)verticalPitch, pSrc->Data.MemId, 0, roi,pDst->Info.FourCC);
            else
                sts = pCmCopy->CopyVideoToSystemMemoryAPI(pDst->Data.Y, pDst->Data.Pitch, (mfxU32)verticalPitch, pSrc->Data.MemId, 0, roi);
            MFX_CHECK_STS(sts);
        }
        else if (m_bCmCopy == true && CM_ALIGNED(pDst->Data.Pitch) && (pDst->Info.FourCC == MFX_FOURCC_P010 && pDst->Info.Shift != pSrc->Info.Shift) && CM_ALIGNED(pDst->Data.Y) && CM_ALIGNED(pDst->Data.UV) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pDst->Info.Height && verticalPitch <= 4096)
        {
            sts = pCmCopy->CopyShiftVideoToSystemMemory(pDst->Data.Y, pDst->Data.Pitch,(mfxU32)verticalPitch,pSrc->Data.MemId, 0, roi, 16-pDst->Info.BitDepthLuma);
            MFX_CHECK_STS(sts);
        }
        else if(m_bCmCopy == true && CM_ALIGNED(pDst->Data.Pitch) && (pDst->Info.FourCC == MFX_FOURCC_RGB4 || pDst->Info.FourCC == MFX_FOURCC_BGR4 || pDst->Info.FourCC == MFX_FOURCC_AYUV) && CM_ALIGNED(IPP_MIN(IPP_MIN(pDst->Data.R,pDst->Data.G),pDst->Data.B)) && roi.height <= 16352 && roi.width <= 16352){
            sts = pCmCopy->CopyVideoToSystemMemoryAPI(IPP_MIN(IPP_MIN(pDst->Data.R,pDst->Data.G),pDst->Data.B), pDst->Data.Pitch, (mfxU32)pSrc->Info.Height, pSrc->Data.MemId, 0, roi);
            MFX_CHECK_STS(sts);
        }
        else if(m_bCmCopy == true && CM_ALIGNED(pDst->Data.Pitch) && pDst->Info.FourCC == MFX_FOURCC_ARGB16 && CM_ALIGNED(IPP_MIN(IPP_MIN(pDst->Data.R,pDst->Data.G),pDst->Data.B)) && roi.height <= 10240 && roi.width <= 10240){
            if(pSrc->Info.FourCC == MFX_FOURCC_ABGR16)
                sts = pCmCopy->CopySwapVideoToSystemMemory(IPP_MIN(IPP_MIN(pDst->Data.R,pDst->Data.G),pDst->Data.B), pDst->Data.Pitch, (mfxU32)pSrc->Info.Height, pSrc->Data.MemId, 0, roi, MFX_FOURCC_ABGR16);
            else
                sts = pCmCopy->CopyVideoToSystemMemoryAPI(IPP_MIN(IPP_MIN(pDst->Data.R,pDst->Data.G),pDst->Data.B), pDst->Data.Pitch, (mfxU32)pSrc->Info.Height, pSrc->Data.MemId, 0, roi);
            MFX_CHECK_STS(sts);
        }
        else if(m_bCmCopy == true && CM_ALIGNED(pDst->Data.Pitch) && pDst->Info.FourCC != MFX_FOURCC_YV12 && pDst->Info.FourCC != MFX_FOURCC_NV12 && pDst->Info.FourCC != MFX_FOURCC_P010 && pDst->Info.FourCC != MFX_FOURCC_A2RGB10 && CM_ALIGNED(pDst->Data.Y) && CM_SUPPORTED_COPY_SIZE(roi)){
            sts = pCmCopy->CopyVideoToSystemMemoryAPI(pDst->Data.Y, pDst->Data.Pitch, (mfxU32)verticalPitch, pSrc->Data.MemId, 0, roi);
            MFX_CHECK_STS(sts);
        }
        else
        {
            MFX_CHECK((pDst->Data.Y == 0) == (pDst->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(dstPitch < 0x8000 || pDst->Info.FourCC == MFX_FOURCC_RGB4 || pDst->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);

            IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *) pSrc->Data.MemId;

            hRes  = pSurface->GetDesc(&sSurfDesc);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "FastCopySSE");
            hRes |= pSurface->LockRect(&sLockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_LOCK_MEMORY);

            Ipp32u srcPitch = sLockRect.Pitch;

            MFX_CHECK(srcPitch < 0x8000 || pSrc->Info.FourCC == MFX_FOURCC_RGB4 || pSrc->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);

            switch (pDst->Info.FourCC)
            {
                case MFX_FOURCC_NV12:

                    sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    roi.height >>= 1;

                    sts = pFastCopy->Copy(pDst->Data.UV, dstPitch, (mfxU8 *)sLockRect.pBits + sSurfDesc.Height * srcPitch, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    break;

                case MFX_FOURCC_P010:

                    if(pDst->Info.Shift == pSrc->Info.Shift){
                        roi.width <<= 1;

                        sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                        MFX_CHECK_STS(sts);

                        roi.height >>= 1;

                        sts = pFastCopy->Copy(pDst->Data.UV, dstPitch, (mfxU8 *)sLockRect.pBits + sSurfDesc.Height * srcPitch, srcPitch, roi);
                        MFX_CHECK_STS(sts);
                    }
                    else if(pDst->Info.Shift == 0)
                    {
                        roi.width <<= 1;

                        sts = pFastCopy->Copy((mfxU16*)(pDst->Data.Y), dstPitch, (mfxU16 *)sLockRect.pBits, srcPitch, roi, 0, (Ipp8u)(16-pDst->Info.BitDepthLuma));
                        MFX_CHECK_STS(sts);

                        roi.height >>= 1;

                        sts = pFastCopy->Copy((mfxU16*)(pDst->Data.UV), dstPitch, (mfxU16 *)sLockRect.pBits + sSurfDesc.Height * srcPitch, srcPitch, roi, 0, (Ipp8u)(16-pDst->Info.BitDepthChroma));
                        MFX_CHECK_STS(sts);
                    }
                    break;
                case MFX_FOURCC_YV12:

                    sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    roi.width >>= 1;
                    roi.height >>= 1;

                    srcPitch >>= 1;
                    dstPitch >>= 1;

                    sts = pFastCopy->Copy(pDst->Data.V, dstPitch, (mfxU8 *)sLockRect.pBits + sSurfDesc.Height * sLockRect.Pitch, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    sts = pFastCopy->Copy(pDst->Data.U, dstPitch, (mfxU8 *)sLockRect.pBits + (sSurfDesc.Height * sLockRect.Pitch * 5) / 4, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    break;

                case MFX_FOURCC_YUY2:

                    roi.width *= 2;

                    sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    break;

                case MFX_FOURCC_RGB3:
                {
                    MFX_CHECK_NULL_PTR1(pDst->Data.R);
                    MFX_CHECK_NULL_PTR1(pDst->Data.G);
                    MFX_CHECK_NULL_PTR1(pDst->Data.B);

                    mfxU8* ptrDst = IPP_MIN(IPP_MIN(pDst->Data.R, pDst->Data.G), pDst->Data.B);

                    roi.width *= 3;

                    sts = pFastCopy->Copy(ptrDst, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);
                }
                    break;

                case MFX_FOURCC_Y410:
                {
                    MFX_CHECK_NULL_PTR1(pDst->Data.Y410);

                    mfxU8* ptrDst = (mfxU8*) pDst->Data.Y410;

                    roi.width *= 4;

                    sts = pFastCopy->Copy(ptrDst, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);
                }
                    break;

                case MFX_FOURCC_A2RGB10:
                {
                    MFX_CHECK_NULL_PTR1(pDst->Data.A2RGB10);

                    mfxU8* ptrDst = (mfxU8*) pDst->Data.A2RGB10;

                    roi.width *= 4;

                    sts = pFastCopy->Copy(ptrDst, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);
                }
                    break;

                case MFX_FOURCC_AYUV:
                case MFX_FOURCC_RGB4:
                {
                    MFX_CHECK_NULL_PTR1(pDst->Data.R);
                    MFX_CHECK_NULL_PTR1(pDst->Data.G);
                    MFX_CHECK_NULL_PTR1(pDst->Data.B);

                    mfxU8* ptrDst = IPP_MIN(IPP_MIN(pDst->Data.R, pDst->Data.G), pDst->Data.B);

                    roi.width *= 4;

                    sts = pFastCopy->Copy(ptrDst, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);
                }
                    break;

                case MFX_FOURCC_ARGB16:
                {
                    MFX_CHECK_NULL_PTR1(pDst->Data.R);
                    MFX_CHECK_NULL_PTR1(pDst->Data.G);
                    MFX_CHECK_NULL_PTR1(pDst->Data.B);

                    mfxU8* ptrDst = IPP_MIN(IPP_MIN(pDst->Data.R, pDst->Data.G), pDst->Data.B);

                    roi.width *= 8;

                    sts = pFastCopy->Copy(ptrDst, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);
                }
                    break;

                case MFX_FOURCC_P8:

                    sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    break;

                /*case MFX_FOURCC_IMC3:
                    sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pBits, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    roi.width >>= 1;
                    roi.height >>= 1;

                    srcPitch >>= 1;
                    dstPitch >>= 1;

                    sts = pFastCopy->Copy(pDst->Data.U, dstPitch, (mfxU8 *)sLockRect.pBits + sSurfDesc.Height * srcPitch, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    sts = pFastCopy->Copy(pDst->Data.V, dstPitch, (mfxU8 *)sLockRect.pBits + (sSurfDesc.Height * srcPitch * 3) / 2, srcPitch, roi);
                    MFX_CHECK_STS(sts);
                    break;*/


                default:

                    return MFX_ERR_UNSUPPORTED;
            }

            hRes = pSurface->UnlockRect();
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
        }
    }
    else if (NULL != pSrc->Data.Y && NULL != pDst->Data.Y)
    {
        // system memories were passed
        // use common way to copy frames

        MFX_CHECK((pSrc->Data.Y == 0) == (pSrc->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK((pDst->Data.Y == 0) == (pDst->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(dstPitch < 0x8000 || pDst->Info.FourCC == MFX_FOURCC_RGB4 || pDst->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(srcPitch < 0x8000 || pSrc->Info.FourCC == MFX_FOURCC_RGB4 || pSrc->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);

        switch (pDst->Info.FourCC)
        {
            case MFX_FOURCC_NV12:

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, pDst->Data.Y, dstPitch, roi);

                roi.height >>= 1;

                ippiCopy_8u_C1R(pSrc->Data.UV, srcPitch, pDst->Data.UV, dstPitch, roi);

                break;

            case MFX_FOURCC_P010:

                roi.width <<= 1;

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, pDst->Data.Y, dstPitch, roi);

                roi.height >>= 1;

                ippiCopy_8u_C1R(pSrc->Data.UV, srcPitch, pDst->Data.UV, dstPitch, roi);

                break;

            case MFX_FOURCC_YV12:

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, pDst->Data.Y, dstPitch, roi);

                roi.width >>= 1;
                roi.height >>= 1;

                srcPitch >>= 1;
                dstPitch >>= 1;

                ippiCopy_8u_C1R(pSrc->Data.V, srcPitch, pDst->Data.V, dstPitch, roi);

                ippiCopy_8u_C1R(pSrc->Data.U, srcPitch, pDst->Data.U, dstPitch, roi);

                break;

            case MFX_FOURCC_YUY2:

                roi.width *= 2;

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, pDst->Data.Y, dstPitch, roi);

                break;

            case MFX_FOURCC_RGB3:
            {
                MFX_CHECK_NULL_PTR1(pSrc->Data.R);
                MFX_CHECK_NULL_PTR1(pSrc->Data.G);
                MFX_CHECK_NULL_PTR1(pSrc->Data.B);

                mfxU8* ptrSrc = IPP_MIN(IPP_MIN(pSrc->Data.R, pSrc->Data.G), pSrc->Data.B);

                MFX_CHECK_NULL_PTR1(pDst->Data.R);
                MFX_CHECK_NULL_PTR1(pDst->Data.G);
                MFX_CHECK_NULL_PTR1(pDst->Data.B);

                mfxU8* ptrDst = IPP_MIN(IPP_MIN(pDst->Data.R, pDst->Data.G), pDst->Data.B);

                roi.width *= 3;

                ippiCopy_8u_C1R(ptrSrc, srcPitch, ptrDst, dstPitch, roi);
            }
                break;

            case MFX_FOURCC_RGB4:
            {
                MFX_CHECK_NULL_PTR1(pSrc->Data.R);
                MFX_CHECK_NULL_PTR1(pSrc->Data.G);
                MFX_CHECK_NULL_PTR1(pSrc->Data.B);

                mfxU8* ptrSrc = IPP_MIN(IPP_MIN(pSrc->Data.R, pSrc->Data.G), pSrc->Data.B);

                MFX_CHECK_NULL_PTR1(pDst->Data.R);
                MFX_CHECK_NULL_PTR1(pDst->Data.G);
                MFX_CHECK_NULL_PTR1(pDst->Data.B);

                mfxU8* ptrDst = IPP_MIN(IPP_MIN(pDst->Data.R, pDst->Data.G), pDst->Data.B);

                roi.width *= 4;

                ippiCopy_8u_C1R(ptrSrc, srcPitch, ptrDst, dstPitch, roi);
            }
                break;

            case MFX_FOURCC_P8:
                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, pDst->Data.Y, dstPitch, roi);
                break;

            /*case MFX_FOURCC_IMC3:

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, pDst->Data.Y, dstPitch, roi);

                roi.width >>= 1;
                roi.height >>= 1;

                srcPitch >>= 1;
                dstPitch >>= 1;

                ippiCopy_8u_C1R(pSrc->Data.U, srcPitch, pDst->Data.U, dstPitch, roi);
                ippiCopy_8u_C1R(pSrc->Data.V, srcPitch, pDst->Data.V, dstPitch, roi);
                break;*/

            default:

                return MFX_ERR_UNSUPPORTED;
        }
    }
    else if (NULL != pSrc->Data.Y && NULL != pDst->Data.MemId)
    {
        // source are placed in system memory, destination is in video memory
        // use common way to copy frames from system to video, most faster

        MFX_CHECK((pSrc->Data.Y == 0) == (pSrc->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(srcPitch < 0x8000 || pSrc->Info.FourCC == MFX_FOURCC_RGB4 || pSrc->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);

        mfxI64 verticalPitch = (mfxI64)(pSrc->Data.UV - pSrc->Data.Y);
        verticalPitch = (verticalPitch % pSrc->Data.Pitch)? 0 : verticalPitch / pSrc->Data.Pitch;

        if (m_bCmCopy == true && CM_ALIGNED(pSrc->Data.Pitch) && (pDst->Info.FourCC == MFX_FOURCC_NV12 || (pDst->Info.FourCC == MFX_FOURCC_P010 && pDst->Info.Shift == pSrc->Info.Shift)) && CM_ALIGNED(pSrc->Data.Y) && CM_ALIGNED(pSrc->Data.UV) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pSrc->Info.Height && verticalPitch <= 16384)
        {
            if(m_HWType >= MFX_HW_SCL)
                sts = pCmCopy->CopySystemToVideoMemory(pDst->Data.MemId, 0, pSrc->Data.Y, pSrc->Data.Pitch,(mfxU32)verticalPitch, roi, pDst->Info.FourCC);
            else
                sts = pCmCopy->CopySystemToVideoMemoryAPI(pDst->Data.MemId, 0, pSrc->Data.Y, pSrc->Data.Pitch,(mfxU32)verticalPitch, roi);
            MFX_CHECK_STS(sts);
        }
        else if (m_bCmCopy == true && CM_ALIGNED(pSrc->Data.Pitch) && (pDst->Info.FourCC == MFX_FOURCC_P010 && pDst->Info.Shift != pSrc->Info.Shift) && CM_ALIGNED(pSrc->Data.Y) && CM_ALIGNED(pSrc->Data.UV) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pSrc->Info.Height && verticalPitch <= 4096)
        {
            sts = pCmCopy->CopyShiftSystemToVideoMemory(pDst->Data.MemId, 0, pSrc->Data.Y, pSrc->Data.Pitch,(mfxU32)verticalPitch, roi, 16 - pSrc->Info.BitDepthLuma);
            MFX_CHECK_STS(sts);
        }
        else if(m_bCmCopy == true && CM_ALIGNED(pSrc->Data.Pitch) && pSrc->Info.FourCC == MFX_FOURCC_RGB4 && CM_ALIGNED(IPP_MIN(IPP_MIN(pSrc->Data.R,pSrc->Data.G),pSrc->Data.B)) && roi.height <= 16352 && roi.width <= 16352){
            if(pDst->Info.FourCC == MFX_FOURCC_BGR4)
                sts = pCmCopy->CopySwapSystemToVideoMemory(pDst->Data.MemId, 0, IPP_MIN(IPP_MIN(pSrc->Data.R,pSrc->Data.G),pSrc->Data.B), pSrc->Data.Pitch, (mfxU32)pSrc->Info.Height, roi, MFX_FOURCC_BGR4);
            else
                sts = pCmCopy->CopySystemToVideoMemoryAPI(pDst->Data.MemId, 0, IPP_MIN(IPP_MIN(pSrc->Data.R,pSrc->Data.G),pSrc->Data.B), pSrc->Data.Pitch, (mfxU32)pSrc->Info.Height, roi);
            MFX_CHECK_STS(sts);
        }
        else if(m_bCmCopy == true && CM_ALIGNED(pSrc->Data.Pitch) && pSrc->Info.FourCC == MFX_FOURCC_BGR4 && CM_ALIGNED(IPP_MIN(IPP_MIN(pSrc->Data.R,pSrc->Data.G),pSrc->Data.B)) && roi.height <= 16352 && roi.width <= 16352){
            sts = pCmCopy->CopySystemToVideoMemoryAPI(pDst->Data.MemId, 0, IPP_MIN(IPP_MIN(pSrc->Data.R,pSrc->Data.G),pSrc->Data.B), pSrc->Data.Pitch, (mfxU32)pSrc->Info.Height, roi);
            MFX_CHECK_STS(sts);
        }
        else if(m_bCmCopy == true && CM_ALIGNED(pSrc->Data.Pitch) && pSrc->Info.FourCC != MFX_FOURCC_YV12 && pSrc->Info.FourCC != MFX_FOURCC_NV12 && pSrc->Info.FourCC != MFX_FOURCC_P010 && CM_ALIGNED(pSrc->Data.Y) && CM_SUPPORTED_COPY_SIZE(roi)){
            sts = pCmCopy->CopySystemToVideoMemoryAPI(pDst->Data.MemId, 0, pSrc->Data.Y, pSrc->Data.Pitch, (mfxU32)verticalPitch, roi);
            MFX_CHECK_STS(sts);
        }
        else
        {
            IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *) pDst->Data.MemId;

        hRes  = pSurface->GetDesc(&sSurfDesc);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        hRes |= pSurface->LockRect(&sLockRect, NULL, NULL);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_LOCK_MEMORY);

        Ipp32u dstPitch = sLockRect.Pitch;

        MFX_CHECK(dstPitch < 0x8000 || pDst->Info.FourCC == MFX_FOURCC_RGB4 || pDst->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);

        switch (pDst->Info.FourCC)
        {
            case MFX_FOURCC_NV12:

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, (mfxU8 *)sLockRect.pBits, dstPitch, roi);

                roi.height >>= 1;

                ippiCopy_8u_C1R(pSrc->Data.UV, srcPitch, (mfxU8 *)sLockRect.pBits + sSurfDesc.Height * dstPitch, dstPitch, roi);

                break;
            case MFX_FOURCC_P010:

                if(pDst->Info.Shift == pSrc->Info.Shift){

                    roi.width <<= 1;

                    ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, (mfxU8 *)sLockRect.pBits, dstPitch, roi);

                    roi.height >>= 1;

                    ippiCopy_8u_C1R(pSrc->Data.UV, srcPitch, (mfxU8 *)sLockRect.pBits + sSurfDesc.Height * dstPitch, dstPitch, roi);
                
                }
                else if(pSrc->Info.Shift == 0)
                {
                    roi.width <<= 1;

                    sts = pFastCopy->Copy((mfxU16*)(pSrc->Data.Y), srcPitch, (mfxU16 *)sLockRect.pBits, dstPitch, roi, (Ipp8u)(16-pDst->Info.BitDepthLuma), 0);
                    MFX_CHECK_STS(sts);

                    roi.height >>= 1;

                    sts = pFastCopy->Copy((mfxU16*)(pSrc->Data.UV), srcPitch, (mfxU16 *)sLockRect.pBits + sSurfDesc.Height * dstPitch, srcPitch, roi, (Ipp8u)(16-pDst->Info.BitDepthChroma), 0);
                    MFX_CHECK_STS(sts);
                }
                break;
            case MFX_FOURCC_YV12:

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, (mfxU8 *)sLockRect.pBits, dstPitch, roi);

                roi.width >>= 1;
                roi.height >>= 1;

                srcPitch >>= 1;
                dstPitch >>= 1;

                ippiCopy_8u_C1R(pSrc->Data.V, srcPitch, (mfxU8 *)sLockRect.pBits + sSurfDesc.Height * sLockRect.Pitch, dstPitch, roi);

                ippiCopy_8u_C1R(pSrc->Data.U, srcPitch, (mfxU8 *)sLockRect.pBits + (sSurfDesc.Height * sLockRect.Pitch * 5) / 4, dstPitch, roi);

                break;

            case MFX_FOURCC_YUY2:

                roi.width *= 2;

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, (mfxU8 *)sLockRect.pBits, dstPitch, roi);

                break;

            case MFX_FOURCC_RGB3:
            {
                MFX_CHECK_NULL_PTR1(pSrc->Data.R);
                MFX_CHECK_NULL_PTR1(pSrc->Data.G);
                MFX_CHECK_NULL_PTR1(pSrc->Data.B);

                mfxU8* ptrSrc = IPP_MIN(IPP_MIN(pSrc->Data.R, pSrc->Data.G), pSrc->Data.B);

                roi.width *= 3;

                ippiCopy_8u_C1R(ptrSrc, srcPitch, (mfxU8 *)sLockRect.pBits, dstPitch, roi);
            }
                break;

            case MFX_FOURCC_AYUV:
            case MFX_FOURCC_RGB4:
            {
                MFX_CHECK_NULL_PTR1(pSrc->Data.R);
                MFX_CHECK_NULL_PTR1(pSrc->Data.G);
                MFX_CHECK_NULL_PTR1(pSrc->Data.B);

                mfxU8* ptrSrc = IPP_MIN(IPP_MIN(pSrc->Data.R, pSrc->Data.G), pSrc->Data.B);

                roi.width *= 4;

                ippiCopy_8u_C1R(ptrSrc, srcPitch, (mfxU8 *)sLockRect.pBits, dstPitch, roi);
            }
                break;

            case MFX_FOURCC_A2RGB10:
            {
                MFX_CHECK_NULL_PTR1(pSrc->Data.R);
                MFX_CHECK_NULL_PTR1(pSrc->Data.G);
                MFX_CHECK_NULL_PTR1(pSrc->Data.B);

                mfxU8* ptrSrc = IPP_MIN(IPP_MIN(pSrc->Data.R, pSrc->Data.G), pSrc->Data.B);

                roi.width *= 4;

                ippiCopy_8u_C1R(ptrSrc, srcPitch, (mfxU8 *)sLockRect.pBits, dstPitch, roi);
            }
                break;

            case MFX_FOURCC_P8:
                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, (mfxU8 *)sLockRect.pBits, dstPitch, roi);
                break;

            /*case MFX_FOURCC_IMC3:
                ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits, srcPitch, pDst->Data.Y, dstPitch, roi);

                roi.width >>= 1;
                roi.height >>= 1;

                srcPitch >>= 1;
                dstPitch >>= 1;

                ippiCopy_8u_C1R(pSrc->Data.U, srcPitch, (mfxU8 *)sLockRect.pBits + sSurfDesc.Height * dstPitch, dstPitch, roi);
                ippiCopy_8u_C1R(pSrc->Data.V, srcPitch, (mfxU8 *)sLockRect.pBits + (sSurfDesc.Height * dstPitch * 3) / 2, dstPitch, roi);
                break;*/


            default:

                return MFX_ERR_UNSUPPORTED;
        }

            hRes = pSurface->UnlockRect();
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
        }
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D9VideoCORE::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)

mfxStatus D3D9VideoCORE::DoFastCopy(mfxFrameSurface1 *dst, mfxFrameSurface1 *src)
{
    CommonCORE::DoFastCopy(dst, src);

    return MFX_ERR_NONE;

}

void D3D9VideoCORE::ReleaseHandle()
{
    if (m_hdl)
    {
        ((IUnknown*)m_hdl)->Release();
        m_bUseExtManager = false;
        m_hdl = m_pDirect3DDeviceManager = 0;
    }
}

mfxStatus D3D9VideoCORE::SetCmCopyStatus(bool enable)
{
    m_bCmCopyAllowed = enable;
    if (!enable)
    {
        if (m_pCmCopy.get())
        {
            m_pCmCopy.get()->Release();
        }
        m_bCmCopy = false;
    }
    return MFX_ERR_NONE;
}

void* D3D9VideoCORE::QueryCoreInterface(const MFX_GUID &guid)
{
    // single instance
    if (MFXIVideoCORE_GUID == guid)
    {
        return (void*) this;
    }
    else if ((MFXICORED3D_GUID == guid))
    {
        return (void*) m_pAdapter.get();
    }
    else if (MFXIHWCAPS_GUID == guid)
    {
        return (void*) &m_encode_caps;
    }
    else if (MFXIHWMBPROCRATE_GUID == guid)
    {
        return (void*) &m_encode_mbprocrate;
    }
    else if (MFXICORECM_GUID == guid)
    {
        CmDevice* pCmDevice = NULL;
        if (!m_bCmCopy)
        {
            m_pCmCopy.reset(new CmCopyWrapper);
            pCmDevice = m_pCmCopy.get()->GetCmDevice<IDirect3DDeviceManager9>(m_pDirect3DDeviceManager);
            if (!pCmDevice)
                return NULL;
            if (MFX_ERR_NONE != m_pCmCopy.get()->Initialize())
                return NULL;
            m_bCmCopy = true;
        }
        else
        {
             pCmDevice =  m_pCmCopy.get()->GetCmDevice<IDirect3DDeviceManager9>(m_pDirect3DDeviceManager);
        }
        return (void*)pCmDevice;
    }
    else if (MFXICORECMCOPYWRAPPER_GUID == guid)
    {
        if (!m_pCmCopy.get())
        {
            m_pCmCopy.reset(new CmCopyWrapper);
            if (!m_pCmCopy.get()->GetCmDevice<IDirect3DDeviceManager9>(m_pDirect3DDeviceManager)){
                //!!!! WA: CM restricts multiple CmDevice creation from different device managers.
                //if failed to create CM device, continue without CmCopy
                m_bCmCopy = false;
                m_bCmCopyAllowed = false;
                m_pCmCopy.get()->Release();
                m_pCmCopy.reset();
                return NULL;
            }else{
                if(MFX_ERR_NONE != m_pCmCopy.get()->Initialize())
                    return NULL;
                else
                    m_bCmCopy = true;
            }
        }
        return (void*)m_pCmCopy.get();
    }
    else if (MFXICMEnabledCore_GUID == guid)
    {
        if (!m_pCmAdapter.get())
        {
            m_pCmAdapter.reset(new CMEnabledCoreAdapter(this));
        }
        return (void*)m_pCmAdapter.get();
    }
    else if (MFXIEXTERNALLOC_GUID == guid && m_bSetExtFrameAlloc)
        return &m_FrameAllocator.frameAllocator;
    else if (MFXICORE_API_1_19_GUID == guid)
    {
        return &m_API_1_19;
    }

    return NULL;
}

bool D3D9VideoCORE::IsCompatibleForOpaq()
{
    if (!m_bUseExtManager)
    {
        return (m_session->m_pScheduler->GetNumRef() > 2)?false:true;
    }
    return true;
}

#endif
/* EOF */
