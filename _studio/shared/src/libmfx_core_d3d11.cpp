/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2013 Intel Corporation. All Rights Reserved.

File Name: libmf_core_d3d.cpp

\* ****************************************************************************** */

#if defined  (MFX_VA)
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

#include "mfx_umc_alloc_wrapper.h"
#include "mfx_common_decode_int.h"
#include "libmfx_core_hw.h"

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report, 
0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);

D3D11VideoCORE::D3D11VideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session)
    :   CommonCORE(numThreadsAvailable, session)
    ,   m_bUseExtAllocForHWFrames(false)
    ,   m_adapterNum(adapterNum)
    ,   m_HWType(MFX_HW_UNKNOWN)
{
}

mfxStatus D3D11VideoCORE::InternalInit()
{
    if (m_HWType != MFX_HW_UNKNOWN)
        return MFX_ERR_NONE;

    mfxU32 platformFromDriver = 0;

    D3D11_VIDEO_DECODER_CONFIG config;
    mfxStatus sts = GetIntelDataPrivateReport(DXVA2_ModeH264_VLD_NoFGT, 0, config);
    if (sts == MFX_ERR_NONE)
        platformFromDriver = config.ConfigBitstreamRaw;

    //need to replace with specific D3D11 approach
    m_HWType = MFX::GetHardwareType(m_adapterNum, platformFromDriver);
    return MFX_ERR_NONE;
}

eMFXHWType D3D11VideoCORE::GetHWType()
{
    InternalInit();
    return m_HWType;
}

mfxStatus D3D11VideoCORE::GetIntelDataPrivateReport(const GUID guid, mfxVideoParam *par, D3D11_VIDEO_DECODER_CONFIG & config)
{
    mfxStatus mfxRes = InternalCreateDevice();
    MFX_CHECK_STS(mfxRes);

    D3D11_VIDEO_DECODER_DESC video_desc = {0};
    D3D11_VIDEO_DECODER_CONFIG video_config = {0};
    video_desc.Guid = DXVADDI_Intel_Decode_PrivateData_Report;
    video_desc.SampleWidth = par ? par->mfx.FrameInfo.Width : 640;
    video_desc.SampleHeight = par ? par->mfx.FrameInfo.Height : 480;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;

    mfxU32 cDecoderProfiles = m_pD11VideoDevice->GetVideoDecoderProfileCount();
    bool isRequestedGuidPresent = false;
    bool isIntelGuidPresent = false;

    for (mfxU32 i = 0; i < cDecoderProfiles; i++)
    {
        GUID decoderGuid;
        HRESULT hr = m_pD11VideoDevice->GetVideoDecoderProfile(i, &decoderGuid);
        if (FAILED(hr))
        {
            continue;
        }

        if (guid == decoderGuid)
            isRequestedGuidPresent = true;

        if (DXVADDI_Intel_Decode_PrivateData_Report == decoderGuid)
            isIntelGuidPresent = true;
    }

    if (!isRequestedGuidPresent)
        return MFX_ERR_NOT_FOUND;

    if (!isIntelGuidPresent) // if no required GUID - no acceleration at all
        return MFX_WRN_PARTIAL_ACCELERATION;

    mfxU32  count = 0;
    HRESULT hr = m_pD11VideoDevice->GetVideoDecoderConfigCount(&video_desc, &count);
    if (FAILED(hr))
        return MFX_WRN_PARTIAL_ACCELERATION;

    for (mfxU32 i = 0; i < count; i++)
    {
        hr = m_pD11VideoDevice->GetVideoDecoderConfig(&video_desc, i, &video_config);
        if (FAILED(hr))
            return MFX_WRN_PARTIAL_ACCELERATION;

        if (video_config.guidConfigBitstreamEncryption == guid)
        {
            memcpy_s(&config, sizeof(config), &video_config, sizeof(D3D11_VIDEO_DECODER_CONFIG));
            return MFX_ERR_NONE;
        }
    }

    return MFX_WRN_PARTIAL_ACCELERATION;
}

mfxStatus D3D11VideoCORE::IsGuidSupported(const GUID guid, mfxVideoParam *par, bool)
{
    if (!par)
        return MFX_WRN_PARTIAL_ACCELERATION;

    mfxStatus sts = InternalInit();
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = InternalCreateDevice();
    if (sts < MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    D3D11_VIDEO_DECODER_CONFIG config;
    sts = GetIntelDataPrivateReport(guid, par, config);

    if (sts < MFX_ERR_NONE || sts == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_WRN_PARTIAL_ACCELERATION;

    if (sts == MFX_ERR_NONE)
    {
        return CheckIntelDataPrivateReport<D3D11_VIDEO_DECODER_CONFIG>(&config, par); 
    }

    if (MFX_HW_LAKE == m_HWType || MFX_HW_SNB == m_HWType)
    {
        if (par->mfx.FrameInfo.Width  > 1920 || par->mfx.FrameInfo.Height > 1200)
            return MFX_WRN_PARTIAL_ACCELERATION;
    }
    else
    {
        if (par->mfx.FrameInfo.Width  > 4096 || par->mfx.FrameInfo.Height > 4096)
            return MFX_WRN_PARTIAL_ACCELERATION;
    }

    return MFX_ERR_NONE;
}
// DX11 support

mfxStatus D3D11VideoCORE::InitializeDevice(bool isTemporal)
{
    mfxStatus sts = InternalInit();
    if (sts != MFX_ERR_NONE)
        return sts;

    sts = InternalCreateDevice();
    if (sts < MFX_ERR_NONE)
        return sts;

    if (m_pD11Device && !m_hdl && !isTemporal)
    {
        m_hdl = (mfxHDL)m_pD11Device;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoCORE::InternalCreateDevice()
{
    if (m_pD11Device)
        return MFX_ERR_NONE;

    HRESULT hres = S_OK;
    static D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    D3D_DRIVER_TYPE type = D3D_DRIVER_TYPE_HARDWARE;
    m_pAdapter = NULL;

    if (m_adapterNum != 0) // not default 
    {
        CreateDXGIFactory(__uuidof(IDXGIFactory), (void**) (&m_pFactory));
        m_pFactory->EnumAdapters(m_adapterNum, &m_pAdapter);
        type = D3D_DRIVER_TYPE_UNKNOWN; // !!!!!! See MSDN, how to create device using not default device
    }

    hres =  D3D11CreateDevice(m_pAdapter,    // provide real adapter
                              type,
                              NULL,
                              0,
                              FeatureLevels,
                              sizeof(FeatureLevels)/sizeof(D3D_FEATURE_LEVEL),
                              D3D11_SDK_VERSION,
                              &m_pD11Device,
                              &pFeatureLevelsOut,
                              &m_pD11Context);

    if (FAILED(hres))
    {
        // lets try to create dx11 device with d9 feature level
        static D3D_FEATURE_LEVEL FeatureLevels9[] = { 
            D3D_FEATURE_LEVEL_9_1,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_3 };

        hres =  D3D11CreateDevice(m_pAdapter,    // provide real adapter
                D3D_DRIVER_TYPE_HARDWARE,
                NULL,
                0,
                FeatureLevels9,
                sizeof(FeatureLevels9)/sizeof(D3D_FEATURE_LEVEL),
                D3D11_SDK_VERSION,
                &m_pD11Device,
                &pFeatureLevelsOut,
                &m_pD11Context);

        if (FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;
    }

    // default device should be thread safe
    CComQIPtr<ID3D10Multithread> p_mt(m_pD11Context);
    if (p_mt)
        p_mt->SetMultithreadProtected(true);
    else
        return MFX_ERR_DEVICE_FAILED;

    if (NULL == (m_pD11VideoDevice = m_pD11Device) || 
        NULL == (m_pD11VideoContext = m_pD11Context))
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    
    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoCORE::CreateDevice()

mfxStatus D3D11VideoCORE::AllocFrames(mfxFrameAllocRequest *request, 
                                      mfxFrameAllocResponse *response)
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
            // initialize sse41 fast copy
            m_pFastCopy.reset(new FastCopy());
            m_pFastCopy.get()->Initialize();

            m_bFastCopy = true;
        }

        // Create Service - first call
        sts = InitializeDevice();
        MFX_CHECK_STS(sts);

        // use common core for sw surface allocation
        if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            return CommonCORE::AllocFrames(request, response);
        else
        {
            // external allocator
            if (m_bSetExtFrameAlloc)
            {

                sts = (*m_FrameAllocator.frameAllocator.Alloc)(m_FrameAllocator.frameAllocator.pthis,&temp_request, response);

                // if external allocator cannot allocate d3d frames - use default memory allocator
                if (MFX_ERR_UNSUPPORTED == sts)
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
                    m_bUseExtAllocForHWFrames = true;
                    //sts = ProcessRenderTargets(request, response, &m_FrameAllocator);
                    //MFX_CHECK_STS(sts);
                    RegisterMids(response, request->Type, false);
                   
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

} // mfxStatus D3D11VideoCORE::AllocFrames



mfxStatus D3D11VideoCORE::DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
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
        return CommonCORE::DefaultAllocFrames(request, response);
    }
    ++m_NumAllocators;
    return sts;
}

mfxStatus D3D11VideoCORE::CreateVA(mfxVideoParam *param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts = MFX_ERR_NONE;
    param; request;

    m_pAccelerator.reset(new MFXD3D11Accelerator(m_pD11VideoDevice.p, m_pD11VideoContext));

    mfxHDLPair Pair;
    // TBD
    sts = GetFrameHDL(response->mids[0], (mfxHDL*)&Pair);
    MFX_CHECK_STS(sts);

    mfxU32 hwProfile = ChooseProfile(param, GetHWType());
    if (!hwProfile)
        return MFX_ERR_UNSUPPORTED;

    if (IS_PROTECTION_ANY(param->Protected))
    {
        m_protectedVA.reset(new UMC::ProtectedVA(param->Protected));
        m_pAccelerator->SetProtectedVA(m_protectedVA.get());
    }

    sts = m_pAccelerator->CreateVideoAccelerator(hwProfile, param, this);
    MFX_CHECK_STS(sts);

    if (IS_PROTECTION_ANY(param->Protected))
    {
        HRESULT hr = m_pAccelerator->GetVideoDecoderDriverHandle(&m_DXVA2DecodeHandle);
        if (FAILED(hr))
        {
            m_pAccelerator->Close();
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else
        m_DXVA2DecodeHandle = NULL;

    m_pAccelerator->GetVideoDecoder(&m_D3DDecodeHandle);
    m_pAccelerator->m_HWPlatform = ConvertMFXToUMCType(m_HWType);
    return sts;
}

mfxStatus D3D11VideoCORE::CreateVideoProcessing(mfxVideoParam * param)
{
    mfxStatus sts = MFX_ERR_NONE;
    #if defined (MFX_ENABLE_VPP)
    m_pVideoProcessing.reset( MfxHwVideoProcessing::CreateVideoProcessing(this) );
    if (m_pVideoProcessing.get() == 0)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    sts = m_pVideoProcessing->CreateDevice(this, param, true);
    #else
    param;
    sts = MFX_ERR_UNSUPPORTED;
    #endif
    return sts;
}

mfxStatus D3D11VideoCORE::ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc)
{
    RegisterMids(response, request->Type, !m_bUseExtAllocForHWFrames, pAlloc);
    m_pcHWAlloc.pop();
    return MFX_ERR_NONE;
}

void* D3D11VideoCORE::QueryCoreInterface(const MFX_GUID &guid)
{
    // single instance
    if (MFXIVideoCORE_GUID == guid)
    {
        return (void*) this;
    }
    if (MFXICORED3D11_GUID == guid)
    {
        if (!m_pid3d11Adapter.get())
        {
            m_pid3d11Adapter.reset(new D3D11Adapter(this));
        }
        return (void*)m_pid3d11Adapter.get();
    }
    else if (MFXIHWCAPS_GUID == guid)
    {
        return (void*) &m_encode_caps;
    }
    else if (MFXIHWMBPROCRATE_GUID == guid)
    {
        return (void*) &m_encode_mbprocrate;
    }
    else if (MFXID3D11DECODER_GUID == guid)
    {
        return (void*)&m_comptr;
    }
    return NULL;
}

mfxStatus D3D11VideoCORE::DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType)
{
    mfxStatus sts;

    mfxHDLPair srcHandle, dstHandle;
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
            sts = GetExternalFrameHDL(srcMemId, (mfxHDL*)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
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
            sts = GetFrameHDL(srcMemId, (mfxHDL*)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
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
            sts = GetExternalFrameHDL(dstMemId, (mfxHDL*)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
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
            sts = GetFrameHDL(dstMemId, (mfxHDL*)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
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

mfxStatus D3D11VideoCORE::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    pDst; pSrc;
    
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
    
    D3D11_TEXTURE2D_DESC sSurfDesc = {0};
    D3D11_MAPPED_SUBRESOURCE sLockRect = {0};

    mfxU32 srcPitch = pSrc->Data.PitchLow + ((mfxU32)pSrc->Data.PitchHigh << 16);
    mfxU32 dstPitch = pDst->Data.PitchLow + ((mfxU32)pDst->Data.PitchHigh << 16);

    FastCopy *pFastCopy = m_pFastCopy.get();

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        ID3D11Texture2D * pSurfaceSrc = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pSrc->Data.MemId)->first);
        ID3D11Texture2D * pSurfaceDst = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pDst->Data.MemId)->first);

        size_t indexSrc = (size_t)((mfxHDLPair*)pSrc->Data.MemId)->second;
        size_t indexDst = (size_t)((mfxHDLPair*)pDst->Data.MemId)->second;

        m_pD11Context->CopySubresourceRegion(pSurfaceDst, (mfxU32)indexDst, 0, 0, 0, pSurfaceSrc, (mfxU32)indexSrc, NULL);

        //should be init m_hDirectXHandle ???
        /*
        hRes = m_pDirect3DDeviceManager->LockDevice(m_hDirectXHandle, &m_pDirect3DDevice, true);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        const tagRECT rect = {0, 0, roi.width, roi.height};

        hRes = m_pDirect3DDevice->StretchRect((IDirect3DSurface9*) pSrc->Data.MemId, &rect, 
                                              (IDirect3DSurface9*) pDst->Data.MemId, &rect, 
                                               D3DTEXF_LINEAR);

        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        hRes = m_pDirect3DDeviceManager->UnlockDevice(m_hDirectXHandle, false);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        m_pDirect3DDevice->Release();
        */
    }
    else if (NULL != pSrc->Data.MemId && NULL != pDst->Data.Y)
    {
        /*
        bFastCompositing = false; // disabled fast copy ddi because of slow performance
        if (bFastCompositing)
        {
            // hardware fast copy

            IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *) pSrc->Data.MemId;

            hRes  = pSurface->GetDesc(&sSurfDesc);

            tagRECT rect = {0, 0, sSurfDesc.Width, sSurfDesc.Height};

            if (NULL == m_pSystemMemorySurface)
            {
                    sts = GetD3DService((mfxU16) roi.width, (mfxU16) roi.height);
                    MFX_CHECK_STS(sts);

                    hRes = m_pDirectXVideoService->CreateSurface(sSurfDesc.Width, sSurfDesc.Height, 0,
                                                                 (D3DFORMAT)pDst->Info.FourCC,
                                                                 D3DPOOL_SYSTEMMEM, 0,
                                                                 DXVA2_VideoSoftwareRenderTarget,
                                                                 &m_pSystemMemorySurface,
                                                                 NULL);

                    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
            }

            // copy from video to system surface
            sts = m_pFastComposing.get()->FastCopy(m_pSystemMemorySurface, pSurface, &rect, &rect, FALSE);
            MFX_CHECK_STS(sts);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyFromD3dPoolSysMem");
            hRes |= m_pSystemMemorySurface->LockRect(&sLockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

            Ipp32u dstPitch = pDst->Data.Pitch;
            Ipp32u srcPitch = sSurfDesc.Width;

            MFX_CHECK((pDst->Data.Y == 0) == (pDst->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(dstPitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(srcPitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);

            switch (pDst->Info.FourCC)
            {
                case MFX_FOURCC_NV12:

                    ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits, srcPitch, pDst->Data.Y, dstPitch, roi);
                            
                    roi.height >>= 1;

                    ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits + sSurfDesc.Height * srcPitch, srcPitch, pDst->Data.UV, dstPitch, roi);

                    break;

                case MFX_FOURCC_YV12:

                    ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits, srcPitch, pDst->Data.Y, dstPitch, roi);

                    roi.width >>= 1;
                    roi.height >>= 1;

                    srcPitch >>= 1;
                    dstPitch >>= 1;

                    ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits + (sSurfDesc.Height * sLockRect.Pitch * 5) / 4, srcPitch, pDst->Data.V, dstPitch, roi);

                    ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits + sSurfDesc.Height * sLockRect.Pitch, srcPitch, pDst->Data.U, dstPitch, roi);
                    
                    break;

                case MFX_FOURCC_YUY2:
                    
                    roi.width *= 2;

                    ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits, srcPitch, pDst->Data.Y, dstPitch, roi);

                    break;

                case MFX_FOURCC_RGB3:

                    roi.width *= 3;

                    ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits, srcPitch, pDst->Data.Y, dstPitch, roi);

                    break;

                case MFX_FOURCC_RGB4:

                    roi.width *= 4;

                    ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits, srcPitch, pDst->Data.Y, dstPitch, roi);

                    break;

                case MFX_FOURCC_P8:

                    ippiCopy_8u_C1R((mfxU8 *)sLockRect.pBits, srcPitch, pDst->Data.Y, dstPitch, roi);

                    break;

                default:

                    return MFX_ERR_UNSUPPORTED;
            }

            hRes = m_pSystemMemorySurface->UnlockRect();
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
        }
        else*/
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

            mfxStatus sts = MFX_ERR_NONE;

            ID3D11Texture2D *pStaging;

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

            Ipp32u srcPitch = sLockRect.RowPitch;

            MFX_CHECK((pDst->Data.Y == 0) == (pDst->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(dstPitch < 0x8000 || pDst->Info.FourCC == MFX_FOURCC_RGB4 || pDst->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(srcPitch < 0x8000 || pSrc->Info.FourCC == MFX_FOURCC_RGB4 || pSrc->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);

            switch (pDst->Info.FourCC)
            {
                case MFX_FOURCC_NV12:

                    sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pData, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    roi.height >>= 1;

                    sts = pFastCopy->Copy(pDst->Data.UV, dstPitch, (mfxU8 *)sLockRect.pData + sSurfDesc.Height * srcPitch, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    break;

                case MFX_FOURCC_YV12:

                    sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pData, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    roi.width >>= 1;
                    roi.height >>= 1;

                    srcPitch >>= 1;
                    dstPitch >>= 1;

                    sts = pFastCopy->Copy(pDst->Data.V, dstPitch, (mfxU8 *)sLockRect.pData + (sSurfDesc.Height * sLockRect.RowPitch * 5) / 4, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    sts = pFastCopy->Copy(pDst->Data.U, dstPitch, (mfxU8 *)sLockRect.pData + sSurfDesc.Height * sLockRect.RowPitch, srcPitch, roi);
                    MFX_CHECK_STS(sts);
                    
                    break;

                case MFX_FOURCC_YUY2:
                    
                    roi.width *= 2;

                    sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pData, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    break;

                case MFX_FOURCC_RGB3:
                {
                    MFX_CHECK_NULL_PTR1(pDst->Data.R);
                    MFX_CHECK_NULL_PTR1(pDst->Data.G);
                    MFX_CHECK_NULL_PTR1(pDst->Data.B);

                    mfxU8* ptrDst = IPP_MIN(IPP_MIN(pDst->Data.R, pDst->Data.G), pDst->Data.B);
                    
                    roi.width *= 3;

                    sts = pFastCopy->Copy(ptrDst, dstPitch, (mfxU8 *)sLockRect.pData, srcPitch, roi);
                    MFX_CHECK_STS(sts);
                }
                    break;

                case MFX_FOURCC_RGB4:
                case DXGI_FORMAT_AYUV:
                {
                    MFX_CHECK_NULL_PTR1(pDst->Data.R);
                    MFX_CHECK_NULL_PTR1(pDst->Data.G);
                    MFX_CHECK_NULL_PTR1(pDst->Data.B);

                    mfxU8* ptrDst = IPP_MIN(IPP_MIN(pDst->Data.R, pDst->Data.G), pDst->Data.B);

                    roi.width *= 4;

                    sts = pFastCopy->Copy(ptrDst, dstPitch, (mfxU8 *)sLockRect.pData, srcPitch, roi);
                    MFX_CHECK_STS(sts);
                }
                    break;

                case MFX_FOURCC_P8:

                    sts = pFastCopy->Copy(pDst->Data.Y, dstPitch, (mfxU8 *)sLockRect.pData, srcPitch, roi);
                    MFX_CHECK_STS(sts);

                    break;

                default:

                    return MFX_ERR_UNSUPPORTED;
            }

            m_pD11Context->Unmap(pStaging, 0);

            SAFE_RELEASE(pStaging);
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
            case DXGI_FORMAT_AYUV:
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

            default:

                return MFX_ERR_UNSUPPORTED;
        }
    }
    else if (NULL != pSrc->Data.Y && NULL != pDst->Data.MemId)
    {
        // source are placed in system memory, destination is in video memory
        // use common way to copy frames from system to video, most faster
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

        Ipp32u dstPitch = sLockRect.RowPitch;

        MFX_CHECK((pSrc->Data.Y == 0) == (pSrc->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(dstPitch < 0x8000 || pDst->Info.FourCC == MFX_FOURCC_RGB4 || pDst->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(srcPitch < 0x8000 || pSrc->Info.FourCC == MFX_FOURCC_RGB4 || pSrc->Info.FourCC == MFX_FOURCC_YUY2, MFX_ERR_UNDEFINED_BEHAVIOR);
        
        switch (pDst->Info.FourCC)
        {
            case MFX_FOURCC_NV12:

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, (mfxU8 *)sLockRect.pData, dstPitch, roi);
                        
                roi.height >>= 1;

                ippiCopy_8u_C1R(pSrc->Data.UV, srcPitch, (mfxU8 *)sLockRect.pData + sSurfDesc.Height * dstPitch, dstPitch, roi);

                break;

            case MFX_FOURCC_YV12:

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, (mfxU8 *)sLockRect.pData, dstPitch, roi);

                roi.width >>= 1;
                roi.height >>= 1;

                srcPitch >>= 1;
                dstPitch >>= 1;

                ippiCopy_8u_C1R(pSrc->Data.V, srcPitch, (mfxU8 *)sLockRect.pData + sSurfDesc.Height * sLockRect.RowPitch, dstPitch, roi);

                ippiCopy_8u_C1R(pSrc->Data.U, srcPitch, (mfxU8 *)sLockRect.pData + (sSurfDesc.Height * sLockRect.RowPitch * 5) / 4, dstPitch, roi);

                break;

            case MFX_FOURCC_YUY2:
                
                roi.width *= 2;

                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, (mfxU8 *)sLockRect.pData, dstPitch, roi);

                break;

            case MFX_FOURCC_RGB3:
            {
                MFX_CHECK_NULL_PTR1(pSrc->Data.R);
                MFX_CHECK_NULL_PTR1(pSrc->Data.G);
                MFX_CHECK_NULL_PTR1(pSrc->Data.B);

                mfxU8* ptrSrc = IPP_MIN(IPP_MIN(pSrc->Data.R, pSrc->Data.G), pSrc->Data.B);

                roi.width *= 3;

                ippiCopy_8u_C1R(ptrSrc, srcPitch, (mfxU8 *)sLockRect.pData, dstPitch, roi);
            }
                break;

            case MFX_FOURCC_RGB4:
            case DXGI_FORMAT_AYUV:
            {
                MFX_CHECK_NULL_PTR1(pSrc->Data.R);
                MFX_CHECK_NULL_PTR1(pSrc->Data.G);
                MFX_CHECK_NULL_PTR1(pSrc->Data.B);

                mfxU8* ptrSrc = IPP_MIN(IPP_MIN(pSrc->Data.R, pSrc->Data.G), pSrc->Data.B);

                roi.width *= 4;

                ippiCopy_8u_C1R(ptrSrc, srcPitch, (mfxU8 *)sLockRect.pData, dstPitch, roi);
            }
                break;

            case MFX_FOURCC_P8:
                ippiCopy_8u_C1R(pSrc->Data.Y, srcPitch, (mfxU8 *)sLockRect.pData, dstPitch, roi);
                break;

            default:

                return MFX_ERR_UNSUPPORTED;
        }

        size_t index = (size_t)((mfxHDLPair*)pDst->Data.MemId)->second;

        m_pD11Context->Unmap(pStaging, 0);
        m_pD11Context->CopySubresourceRegion(pSurface, (mfxU32)index, 0, 0, 0, pStaging, 0, NULL);

        SAFE_RELEASE(pStaging);
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    
    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoCORE::GetDX11Handle(mfxI32 index, mfxHDLPair* pair)
{
    return GetFrameHDL(m_pWrp->ConvertMemId(index), (mfxHDL*)pair);
}

mfxStatus D3D11VideoCORE::SetHandle(mfxHandleType type, mfxHDL handle)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    try
    {
        // SetHandle should be first since 1.6 version
        bool isRequeredEarlySetHandle = (m_session->m_version.Major > 1 ||
            (m_session->m_version.Major == 1 && m_session->m_version.Minor >= 6));

        if ((type == MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 || type == MFX_HANDLE_D3D11_DEVICE) && isRequeredEarlySetHandle && m_pD11Device)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        mfxStatus sts = CommonCORE::SetHandle(type, handle);
        MFX_CHECK_STS(sts);
        #if defined (MFX_ENABLE_VPP)
        if(m_pD11Device)
            m_pVideoProcessing.reset();
        #endif
        m_pD11Device = (ID3D11Device*)m_hdl;
        CComPtr<ID3D11DeviceContext> pImmediateContext;
        m_pD11Device->GetImmediateContext(&pImmediateContext);
        m_pD11Context = pImmediateContext;

        m_pD11VideoDevice = m_pD11Device;
        m_pD11VideoContext = m_pD11Context;


        if (!m_pD11Context)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        CComQIPtr<ID3D10Multithread> p_mt(m_pD11Context);
        if (p_mt)
        {
            if (!p_mt->GetMultithreadProtected())
            {
                ReleaseHandle();
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        else {
            ReleaseHandle();
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }
    catch(MFX_CORE_CATCH_TYPE)
    {
        ReleaseHandle();
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    }

    return MFX_ERR_NONE;

}

    
void D3D11VideoCORE::ReleaseHandle()
{
    if (m_hdl)
    {
        ((IUnknown*)m_hdl)->Release();
        m_bUseExtManager = false;
        m_hdl = 0;
    }
}

bool D3D11VideoCORE::IsCompatibleForOpaq()
{
    if (!m_bUseExtManager)
    {
        return (m_session->m_pScheduler->GetNumRef() > 2)?false:true;
    }
    return true;
}

#endif
#endif 
