/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_d3d11.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_sw_d3d11.h"
#include "mfx_utils.h"

#include <d3d9.h>

namespace MfxCapture
{

bool HasWDDMDriver()
{
    typedef HRESULT (WINAPI *LPDIRECT3DCREATE9EX)( UINT, void **);

    wchar_t path[MAX_PATH + 1] = {};
    size_t len = GetSystemDirectory(path, MAX_PATH);
    if(0 == len)
        return false;
    const wchar_t backslash[] = L"\\";
    const size_t backslash_len = sizeof(backslash)/sizeof(backslash[0]);
    const wchar_t libName[] = L"d3d9.dll";
    const size_t libName_len = sizeof(libName)/sizeof(libName[0]);

    if(len >= wcsnlen_s(backslash, backslash_len))
    {
        const wchar_t* tail = path + len - wcsnlen_s(backslash, backslash_len);
        if((0 != wcscmp(tail, backslash)) && (len + wcsnlen_s(backslash, backslash_len) < MAX_PATH))
        {
            wcscat_s(path, backslash);
            len += wcsnlen_s(backslash, backslash_len);
        }
    }

    if((len + wcsnlen_s(libName, libName_len)) < MAX_PATH)
        wcscat_s(path, libName);
    else
        return false;

    LPDIRECT3DCREATE9EX pD3D9Create9Ex = 0;
    HMODULE             hD3D9          = 0;

    hD3D9 = LoadLibrary( path );

    if ( 0 == hD3D9 ) {
        return false;
    }

    //
    /*  Try to create IDirect3D9Ex interface (also known as a DX9L interface). This interface can only be created if the driver is a WDDM driver.
     */
    //
    pD3D9Create9Ex = (LPDIRECT3DCREATE9EX) GetProcAddress( hD3D9, "Direct3DCreate9Ex" );
    bool WDDM = (pD3D9Create9Ex != 0) ? true : false;

    FreeLibrary(hD3D9);
    hD3D9 = 0;

    return WDDM;
}

/*
SW_D3D11_Capturer means that we will use standard MS DXGI interface instead of internal Intel getDesktop.ddi

IDXGIAdapter - a display device (GPU);
  ID3D11Device - a virtual adapter; it is used to create resources; this one is provided by Set/Get Handle() function;
  IDXGIOutput  - an adapter (GPU) output (only connected displays, at least for Intel GPU); created from IDXGIAdapter interface;
    IDXGIOutputDuplication - an interface from IDXGIOutput to capture the display image; this interface is used for display capturing.
*/

SW_D3D11_Capturer::SW_D3D11_Capturer(mfxCoreInterface* _core)
    :m_pmfxCore(_core)
{
    Mode = SW_D3D11;
    m_DisplayIndex = 0;
}

SW_D3D11_Capturer::~SW_D3D11_Capturer()
{
    Destroy();
}

mfxStatus SW_D3D11_Capturer::CreateVideoAccelerator( mfxVideoParam const & par, const mfxU32 targetId)
{
    /* IDXGIAdapter -> ID3D11Device -> IDXGIOutput(dispIndex) -> IDXGIOutputDuplication */

    try
    {
        if(!HasWDDMDriver())
            return MFX_ERR_UNSUPPORTED;

        HRESULT hres;

        mfxStatus mfxRes = MFX_ERR_NONE;

        m_bOwnDevice = false;

        //will own internal device manager
        memset(&m_core_par, 0, sizeof(m_core_par));
        if(m_pmfxCore)
            mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &m_core_par);

        //in case of partial acceleration try to use library device
        if( !m_pmfxCore || mfxRes || MFX_IMPL_HARDWARE != MFX_IMPL_BASETYPE(m_core_par.Impl) ||
            MFX_IMPL_VIA_D3D11 != (m_core_par.Impl & 0xF00))
            m_bOwnDevice = true;

        displaysDescr displays;
        memset(&displays,0,sizeof(displays));
        FindAllConnectedDisplays(displays);

        m_DisplayIndex = 0;
        if(targetId/* && 1 != dispIndex*/)
        {
            m_DisplayIndex = GetDisplayIndex(targetId);
            if((m_DisplayIndex) && ( MAX_DISPLAYS <= m_DisplayIndex))
            {
                return MFX_ERR_UNSUPPORTED;
            }
            if(displays.n < m_DisplayIndex)
            {
                return MFX_ERR_NOT_FOUND;
            }
            //m_bOwnDevice = true;
        }

        if(!m_bOwnDevice)
        {
            mfxRes = AttachToLibraryDevice(displays, m_DisplayIndex);
            if(mfxRes)
                m_bOwnDevice = true;
        }

        if(m_bOwnDevice)
        {
            mfxRes = CreateAdapter(displays, m_DisplayIndex);
            MFX_CHECK_STS(mfxRes);
        }

        if(!m_pDXGIOutputDuplication)
            return MFX_ERR_DEVICE_FAILED;

        DXGI_OUTDUPL_DESC dupl_desc;
        m_pDXGIOutputDuplication->GetDesc(&dupl_desc);

        if(m_pDesktopResource)
            m_pDesktopResource = 0;

        DXGI_OUTDUPL_FRAME_INFO frame_info;
        hres = m_pDXGIOutputDuplication->AcquireNextFrame(50, &frame_info, &m_pDesktopResource);

        //normally first call should always work
        //call capture at init to 1) Check; 2) Store the first surface
        if (SUCCEEDED(hres) && m_pDesktopResource)
        {
            ID3D11Texture2D* DesktopCaptured = 0;

            hres = m_pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&DesktopCaptured);
            if(FAILED(hres) || !DesktopCaptured)
            {
                if(m_pDesktopResource)
                {
                    m_pDesktopResource = 0;
                }
                if(DesktopCaptured)
                {
                    DesktopCaptured->Release();
                    DesktopCaptured = 0;
                }
                return MFX_ERR_DEVICE_FAILED;
            }

            D3D11_TEXTURE2D_DESC texDesc;
            D3D11_TEXTURE2D_DESC sysDesc;
            memset(&sysDesc, 0, sizeof(sysDesc));
            memset(&texDesc, 0, sizeof(texDesc));
            DesktopCaptured->GetDesc(&texDesc);

            sysDesc = texDesc;
            sysDesc.ArraySize      = 1;
            sysDesc.Usage          = D3D11_USAGE_STAGING;
            sysDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sysDesc.BindFlags      = 0;
            sysDesc.MiscFlags      = 0;

            ID3D11Texture2D* stagingTexture = 0;

            hres = m_pD11Device->CreateTexture2D(&sysDesc, NULL, &stagingTexture);
            if(FAILED(hres))
                return MFX_ERR_MEMORY_ALLOC;

            if(!m_pLastCaptured)
            {
                texDesc.MiscFlags = 0;
                hres = m_pD11Device->CreateTexture2D(&texDesc, NULL, &m_pLastCaptured);
            }
            m_pD11Context->CopyResource(m_pLastCaptured, DesktopCaptured);
            m_pD11Context->CopyResource(stagingTexture, DesktopCaptured);

            D3D11_MAPPED_SUBRESOURCE    lockedRect = {0};
            D3D11_MAP   mapType  = D3D11_MAP_READ;
            UINT        mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;
            do
            {
                hres = m_pD11Context->Map(stagingTexture, 0, mapType, mapFlags, &lockedRect);
                if (S_OK != hres && DXGI_ERROR_WAS_STILL_DRAWING != hres)
                    return MFX_ERR_LOCK_MEMORY;
            }
            while (DXGI_ERROR_WAS_STILL_DRAWING == hres);

            m_pD11Context->Unmap(stagingTexture, 0);

            if(DesktopCaptured)
            {
                DesktopCaptured->Release();
                DesktopCaptured = 0;
            }
            if(m_pDesktopResource)
            {
                m_pDesktopResource = 0;
            }
            if(stagingTexture)
            {
                stagingTexture->Release();
                stagingTexture = 0;
            }

            m_pDXGIOutputDuplication->ReleaseFrame();
        }
        else
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        if(MFX_FOURCC_NV12 == par.mfx.FrameInfo.FourCC)
        {
            try
            {
                m_pColorConverter.reset(new MFXVideoVPPColorSpaceConversion(0, &mfxRes));
            }
            catch(...)
            {
                Destroy();
                return MFX_ERR_MEMORY_ALLOC;
            }
            if(mfxRes)
            {
                Destroy();
                return mfxRes;
            }
            mfxFrameInfo in = par.mfx.FrameInfo;
            mfxFrameInfo out = par.mfx.FrameInfo;
            in.FourCC = MFX_FOURCC_RGB4;
            m_pColorConverter->Init(&in, &out);
        }
    }
    catch(...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

mfxStatus SW_D3D11_Capturer::QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out, const mfxU32 )
{
    in;
    out;
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D11_Capturer::CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out)
{
    if(!(MFX_FOURCC_NV12      == in.mfx.FrameInfo.FourCC ||
         MFX_FOURCC_RGB4      == in.mfx.FrameInfo.FourCC ||
         MFX_FOURCC_AYUV_RGB4 == in.mfx.FrameInfo.FourCC ||
         DXGI_FORMAT_AYUV     == in.mfx.FrameInfo.FourCC ))
    {
        if(out) out->mfx.FrameInfo.FourCC = 0;
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus SW_D3D11_Capturer::Destroy()
{
    try
    {
        if(m_pColorConverter.get())
            m_pColorConverter.reset(0);
        if(m_IntStatusList.size())
            m_IntStatusList.clear();

    #define RELEASE(PTR) { if (PTR) { PTR.Release(); PTR = NULL; } };
        RELEASE(m_pDXGIFactory          );
        RELEASE(m_pDXGIAdapter          );
        RELEASE(m_pDXGIOutput           );
        RELEASE(m_pDXGIOutput1          );
        RELEASE(m_pDXGIOutputDuplication);
        RELEASE(m_pD11Device            );
        RELEASE(m_pDXGIDevice           );
        RELEASE(m_pD11Context           );
        RELEASE(m_pDesktopResource      );
        RELEASE(m_pLastCaptured         );
        RELEASE(m_stagingTexture        );
    #undef RELEASE

        m_bOwnDevice = false;
        m_DisplayIndex = 0;
    }
    catch(...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

mfxStatus SW_D3D11_Capturer::BeginFrame( mfxFrameSurface1* pSurf)
{
    pSurf;
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D11_Capturer::EndFrame( )
{
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D11_Capturer::GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber)
{
    if(!m_pDXGIOutputDuplication || !m_pD11Device || !m_pD11Context || !m_pmfxCore)
        return MFX_ERR_NOT_INITIALIZED;

    HRESULT hr = S_OK;
    mfxStatus mfxRes = MFX_ERR_NONE;

    DXGI_OUTDUPL_FRAME_INFO frame_info;
    if(m_pDesktopResource)
        m_pDesktopResource = 0;

    UMC::AutomaticUMCMutex guard(m_guard);

    hr = m_pDXGIOutputDuplication->AcquireNextFrame(10, &frame_info, &m_pDesktopResource);
    if (DXGI_ERROR_ACCESS_LOST == hr)//restart
    {
        m_pDXGIOutputDuplication = 0;
        m_pDesktopResource = 0;
        if(!m_pDXGIOutput1)
            return MFX_ERR_DEVICE_LOST;

        hr = m_pDXGIOutput1->DuplicateOutput(m_pD11Device, &m_pDXGIOutputDuplication);
        if(FAILED(hr))
            return MFX_ERR_DEVICE_FAILED;

        hr = m_pDXGIOutputDuplication->AcquireNextFrame(10, &frame_info, &m_pDesktopResource);
    }

    if(DXGI_ERROR_WAIT_TIMEOUT == hr) //no display update
    {
        if(!m_pLastCaptured)
            return MFX_ERR_DEVICE_FAILED;
    }
    else if (FAILED(hr)) //failed
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    else //capture succeeded
    {
        ID3D11Texture2D* DesktopCaptured = 0;

        if(FAILED(hr = m_pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&DesktopCaptured)))
        {
            if(DesktopCaptured)
            {
                DesktopCaptured->Release();
                DesktopCaptured = 0;
            }
            return MFX_ERR_DEVICE_FAILED;
        }

        if(!m_pLastCaptured)
        {
            D3D11_TEXTURE2D_DESC texDesc;
            memset(&texDesc, 0, sizeof(texDesc));
            DesktopCaptured->GetDesc(&texDesc);
            texDesc.MiscFlags = 0;
            hr = m_pD11Device->CreateTexture2D(&texDesc, NULL, &m_pLastCaptured);
        }

        m_pD11Context->CopyResource(m_pLastCaptured, DesktopCaptured);

        if(DesktopCaptured)
        {
            DesktopCaptured->Release();
            DesktopCaptured = 0;
        }

        if(m_pDesktopResource)
        {
            m_pDesktopResource.Release();
            m_pDesktopResource = 0;
        }

        m_pDXGIOutputDuplication->ReleaseFrame();
    }

    //Copy from internal surface into MFX

    const mfxMemId MemId = surface_work->Data.MemId;
    mfxHDLPair Pair = {0,0};
    mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, MemId, (mfxHDL*)&Pair);
    if(!m_bOwnDevice && !mfxRes && Pair.first && (MFX_FOURCC_RGB4 == surface_work->Info.FourCC))
    {
        //go through DX copy
        //ID3D11Texture2D* dstTxt = (ID3D11Texture2D*) Pair.first;

        m_pD11Context->CopySubresourceRegion((ID3D11Resource*) Pair.first, (UINT)(size_t)Pair.second, 0,0,0,m_pLastCaptured,0,0);
    }
    else
    {
        D3D11_TEXTURE2D_DESC texDesc;
        memset(&texDesc, 0, sizeof(texDesc));
        m_pLastCaptured->GetDesc(&texDesc);
        if(!m_stagingTexture)
        {
            texDesc.ArraySize      = 1;
            texDesc.Usage          = D3D11_USAGE_STAGING;
            texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            texDesc.BindFlags      = 0;
            texDesc.MiscFlags      = 0;

            hr = m_pD11Device->CreateTexture2D(&texDesc, NULL, &m_stagingTexture);
        }
        m_pD11Context->CopyResource(m_stagingTexture, m_pLastCaptured);

        D3D11_MAPPED_SUBRESOURCE    lockedRect = {0};
        D3D11_MAP   mapType  = D3D11_MAP_READ;
        UINT        mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;

        do
        {
            hr = m_pD11Context->Map(m_stagingTexture, 0, mapType, mapFlags, &lockedRect);
            if (S_OK != hr && DXGI_ERROR_WAS_STILL_DRAWING != hr)
                return MFX_ERR_LOCK_MEMORY;
        }
        while (DXGI_ERROR_WAS_STILL_DRAWING == hr);

        const bool unlock_out = surface_work->Data.Y ? false : true;
        if(unlock_out)
        {
            mfxRes = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, &surface_work->Data);
            if(mfxRes)
                return MFX_ERR_LOCK_MEMORY;
        }

        if(MFX_FOURCC_NV12 == surface_work->Info.FourCC)
        {
            if(!surface_work->Data.Y || !surface_work->Data.UV)
                return MFX_ERR_LOCK_MEMORY;

            mfxFrameSurface1 rgb_surface = *surface_work;
            rgb_surface.Info.FourCC = MFX_FOURCC_RGB4;
            rgb_surface.Info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            const mfxU16 w = IPP_MIN((mfxU16) texDesc.Width, surface_work->Info.Width);
            const mfxU16 h = IPP_MIN((mfxU16) texDesc.Height, surface_work->Info.Height);
            rgb_surface.Info.Width  = rgb_surface.Info.CropW = w;
            rgb_surface.Info.Height = rgb_surface.Info.CropH = h;
            rgb_surface.Data.PitchHigh = (mfxU16)(lockedRect.RowPitch / (1 << 16));
            rgb_surface.Data.PitchLow  = (mfxU16)(lockedRect.RowPitch % (1 << 16));
            rgb_surface.Data.B = (mfxU8 *)lockedRect.pData;
            rgb_surface.Data.G = rgb_surface.Data.B + 1;
            rgb_surface.Data.R = rgb_surface.Data.B + 2;
            rgb_surface.Data.A = rgb_surface.Data.B + 3;

            FilterVPP::InternalParam param = {};
            param.inPicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            if(!m_pColorConverter.get())
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            mfxRes = m_pColorConverter->RunFrameVPP(&rgb_surface,surface_work,&param);
            if(mfxRes)
                return MFX_ERR_DEVICE_FAILED;
        }
        else //copy RGB4 surface
        {
            if(!surface_work->Data.R || !surface_work->Data.G || !surface_work->Data.B)
                return MFX_ERR_LOCK_MEMORY;

            mfxU32 i = 0;
            mfxU32 src_h = texDesc.Height;
            mfxU32 src_w = texDesc.Width;
            mfxU32 dst_h = surface_work->Info.CropH ? surface_work->Info.CropH : surface_work->Info.Height;
            mfxU32 dst_w = surface_work->Info.CropW ? surface_work->Info.CropW : surface_work->Info.Width;
            if(src_h > dst_h || src_w > dst_w)
            {
                src_h = dst_h;
                src_w = dst_w;
            }
            mfxU32 src_pitch = lockedRect.RowPitch;
            mfxU32 dst_pitch = surface_work->Data.Pitch;

            mfxU8* src   = (mfxU8*) lockedRect.pData;
            mfxU8* dst   = IPP_MIN( IPP_MIN(surface_work->Data.R, surface_work->Data.G), surface_work->Data.B );

            for (i = 0; i < src_h; i++)
            {
                memcpy_s(dst + i*dst_pitch, 4*dst_w, src + i*src_pitch, 4*src_w);
            }
        }
        if(unlock_out)
        {
            mfxRes = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, &surface_work->Data);
            if(mfxRes)
                return MFX_ERR_LOCK_MEMORY;
        }

        m_pD11Context->Unmap(m_stagingTexture, 0);
    }

    DESKTOP_QUERY_STATUS_PARAMS status;
    status.StatusReportFeedbackNumber = StatusReportFeedbackNumber;
    status.uStatus = 0;
    m_IntStatusList.push_back(status);

    return MFX_ERR_NONE;
}

mfxStatus SW_D3D11_Capturer::QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList)
{
    if(StatusList.size())
        StatusList.clear();

    StatusList = m_IntStatusList;
    m_IntStatusList.clear();
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D11_Capturer::AttachToLibraryDevice(const displaysDescr& ActiveDisplays, const UINT OutputId)
{
    mfxStatus mfxRes;
    HRESULT hres;

    if(OutputId >= MAX_DISPLAYS)
        return MFX_ERR_UNSUPPORTED;
    if(OutputId >= ActiveDisplays.n)
        return MFX_ERR_NOT_FOUND;

    mfxHDL hdl = 0;
    mfxRes = m_pmfxCore->GetHandle(m_pmfxCore->pthis, MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&hdl);
    if(mfxRes || !hdl)
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    else
    {
        m_pD11Device = (ID3D11Device*) hdl;
        //m_pD11Device.Attach((ID3D11Device*) hdl);

        m_pD11Device->GetImmediateContext(&m_pD11Context);

        // Get DXGI device
        hres = m_pD11Device->QueryInterface(__uuidof(IDXGIDevice2), (void **)&m_pDXGIDevice);
        if(FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;

        // Get DXGI adapter
        hres = m_pDXGIDevice->GetParent(__uuidof(IDXGIAdapter1), reinterpret_cast<void**>(&m_pDXGIAdapter));
        if(FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;

        {
            DXGI_ADAPTER_DESC desc;
            memset(&desc, 0, sizeof(desc));

            m_pDXGIAdapter->GetDesc(&desc);

            if(!(ActiveDisplays.display[OutputId].DXGIAdapterLuid.HighPart == desc.AdapterLuid.HighPart &&
                 ActiveDisplays.display[OutputId].DXGIAdapterLuid.LowPart  == desc.AdapterLuid.LowPart)    )
            {
                //display connected to a different device
                return MFX_ERR_NOT_FOUND;
            }
        }

        DXGI_OUTPUT_DESC outDesc;
        memset(&outDesc,0,sizeof(outDesc));
        UINT i = 0;
        while(m_pDXGIAdapter->EnumOutputs(i, &m_pDXGIOutput) != DXGI_ERROR_NOT_FOUND) 
        {
            if(!m_pDXGIOutput)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            m_pDXGIOutput->GetDesc(&outDesc);

            if(ActiveDisplays.display[OutputId].position.x == outDesc.DesktopCoordinates.left &&
                ActiveDisplays.display[OutputId].position.y == outDesc.DesktopCoordinates.top)
            {
                break;
            }
            //OutputId

            m_pDXGIOutput = 0;
            ++i;
            if(i > MAX_DISPLAYS)
                break;
        }

        hres = m_pDXGIOutput->QueryInterface(IID_IDXGIOutput1, (void**) &m_pDXGIOutput1);
        if(FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;

        hres = m_pDXGIOutput1->DuplicateOutput(m_pD11Device, &m_pDXGIOutputDuplication);
        if(FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;

        mfxRes = MFX_ERR_NONE;
    }

    return mfxRes;
}

mfxStatus SW_D3D11_Capturer::CreateAdapter(const displaysDescr& ActiveDisplays, const UINT OutputId)
{
    //Because DXGI is only available on systems with WDDM drivers, the app must first confirm the driver model by using the following API. 
    if(!HasWDDMDriver())
        return MFX_ERR_UNSUPPORTED;

    if(OutputId >= MAX_DISPLAYS)
        return MFX_ERR_UNSUPPORTED;
    if(OutputId >= ActiveDisplays.n)
        return MFX_ERR_NOT_FOUND;

    HRESULT hres = CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)(&m_pDXGIFactory) );
    if (FAILED(hres)) 
        return MFX_ERR_DEVICE_FAILED;

    DXGI_ADAPTER_DESC desc;
    memset(&desc, 0, sizeof(desc));

    mfxU32 i = 0;
    m_pDXGIAdapter = 0;
    m_pD11Device = 0;
    m_pD11Context = 0;
    m_pDXGIOutput = 0;
    m_pDXGIOutput1 = 0;
    m_pDXGIOutputDuplication = 0;
    while(m_pDXGIFactory->EnumAdapters1(i, &m_pDXGIAdapter) != DXGI_ERROR_NOT_FOUND) 
    {
        if(!m_pDXGIAdapter)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        m_pDXGIAdapter->GetDesc(&desc);

        //if(desc.VendorId == 0x8086)
        //    break;
        if(ActiveDisplays.display[OutputId].DXGIAdapterLuid.HighPart == desc.AdapterLuid.HighPart &&
           ActiveDisplays.display[OutputId].DXGIAdapterLuid.LowPart  == desc.AdapterLuid.LowPart)
        {
            break;
        }

        m_pDXGIAdapter = 0;
        ++i; 
    }

    if(!m_pDXGIAdapter)
        return MFX_ERR_NOT_FOUND;

    static D3D_FEATURE_LEVEL FeatureLevels[] = {
        //D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    UINT dxFlags = 0;

    hres =  D3D11CreateDevice(  m_pDXGIAdapter,
                                /*D3D_DRIVER_TYPE_HARDWARE*/D3D_DRIVER_TYPE_UNKNOWN,
                                NULL,
                                dxFlags,
                                FeatureLevels,
                                (sizeof(FeatureLevels) / sizeof(FeatureLevels[0])),
                                D3D11_SDK_VERSION,
                                &m_pD11Device,
                                &pFeatureLevelsOut,
                                &m_pD11Context);

    //hres = m_pD11Device->QueryInterface(__uuidof(ID3D11Device1), (void **)&m_pD11Device1);

    CComQIPtr<ID3D10Multithread> p_mt(m_pD11Context);
    if (p_mt)
        p_mt->SetMultithreadProtected(true);

    if(!m_pDXGIDevice)
    {
        hres = m_pD11Device->QueryInterface(__uuidof(IDXGIDevice2), (void **)&m_pDXGIDevice);
    }

    DXGI_OUTPUT_DESC outDesc;
    memset(&outDesc,0,sizeof(outDesc));
    i = 0;
    while(m_pDXGIAdapter->EnumOutputs(i, &m_pDXGIOutput) != DXGI_ERROR_NOT_FOUND) 
    {
        if(!m_pDXGIOutput)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        m_pDXGIOutput->GetDesc(&outDesc);

        if(ActiveDisplays.display[OutputId].position.x == outDesc.DesktopCoordinates.left &&
            ActiveDisplays.display[OutputId].position.y == outDesc.DesktopCoordinates.top)
        {
            break;
        }
        //OutputId

        m_pDXGIOutput = 0;
        ++i;
        if(i > MAX_DISPLAYS)
            break;
    }

    hres = m_pDXGIOutput->QueryInterface(IID_IDXGIOutput1, (void**) &m_pDXGIOutput1);
    if(FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    hres = m_pDXGIOutput1->DuplicateOutput(m_pD11Device, &m_pDXGIOutputDuplication);
    if(FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    return MFX_ERR_NONE;
}

} //namespace MfxCapture