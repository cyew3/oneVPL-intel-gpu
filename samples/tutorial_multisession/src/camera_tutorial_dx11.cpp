/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "camera_tutorial.h"

#include<map>

CComPtr<ID3D11Device>                   g_pD3D11Device;
CComPtr<ID3D11DeviceContext>            g_pD3D11Ctx;
CComPtr<IDXGIFactory2>                  g_pDXGIFactory;
IDXGIAdapter*                           g_pAdapter;

std::map<mfxMemId*, mfxHDL>             allocResponses_dx11;
std::map<mfxHDL, mfxFrameAllocResponse> allocDecodeResponses_dx11;
std::map<mfxHDL, int>                   allocDecodeRefCount_dx11;

typedef struct {
    mfxMemId    memId;
    mfxMemId    memIdStage;
    mfxU16      rw;
} CustomMemId;

const struct {
    mfxIMPL impl;       // actual implementation
    mfxU32  adapterID;  // device adapter number
} implTypes[] = {
    {MFX_IMPL_HARDWARE, 0},
    {MFX_IMPL_HARDWARE2, 1},
    {MFX_IMPL_HARDWARE3, 2},
    {MFX_IMPL_HARDWARE4, 3}
};

// =================================================================
// DirectX functionality required to manage DX11 device and surfaces
//

IDXGIAdapter* GetIntelDeviceAdapterHandle(mfxSession session)
{
    mfxU32  adapterNum = 0;
    mfxIMPL impl;

    MFXQueryIMPL(session, &impl);

    mfxIMPL baseImpl = MFX_IMPL_BASETYPE(impl); // Extract Media SDK base implementation type

    // get corresponding adapter number
    for (mfxU8 i = 0; i < sizeof(implTypes)/sizeof(implTypes[0]); i++)
    {
        if (implTypes[i].impl == baseImpl)
        {
            adapterNum = implTypes[i].adapterID;
            break;
        }
    }

    HRESULT hres = CreateDXGIFactory(__uuidof(IDXGIFactory2), (void**)(&g_pDXGIFactory) );
    if (FAILED(hres)) return NULL;

    IDXGIAdapter* adapter;
    hres = g_pDXGIFactory->EnumAdapters(adapterNum, &adapter);
    if (FAILED(hres)) return NULL;

    return adapter;
}

// Create HW device context
mfxStatus CreateHWDevice_dx11(mfxSession session, mfxHDL* deviceHandle, HWND hWnd)
{
    hWnd; // Window handle not required by DX11 since we do not showcase rendering.

    HRESULT hres = S_OK;

    static D3D_FEATURE_LEVEL FeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    g_pAdapter = GetIntelDeviceAdapterHandle(session);
    if(NULL == g_pAdapter)
        return MFX_ERR_DEVICE_FAILED;

    UINT dxFlags = 0;
    //UINT dxFlags = D3D11_CREATE_DEVICE_DEBUG;

    hres =  D3D11CreateDevice(  g_pAdapter,
                                D3D_DRIVER_TYPE_UNKNOWN,
                                NULL,
                                dxFlags,
                                FeatureLevels,
                                (sizeof(FeatureLevels) / sizeof(FeatureLevels[0])),
                                D3D11_SDK_VERSION,
                                &g_pD3D11Device,
                                &pFeatureLevelsOut,
                                &g_pD3D11Ctx);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    // turn on multithreading for the DX11 context
    CComQIPtr<ID3D10Multithread> p_mt(g_pD3D11Ctx);
    if (p_mt)
        p_mt->SetMultithreadProtected(true);
    else
        return MFX_ERR_DEVICE_FAILED;

 //   *deviceHandle = (mfxHDL)g_pD3D11Device;
    *deviceHandle = g_pD3D11Device.p;

    return MFX_ERR_NONE;
}


void SetHWDeviceContext(CComPtr<ID3D11DeviceContext> devCtx)
{
    g_pD3D11Ctx = devCtx;
    devCtx->GetDevice(&g_pD3D11Device);
}

// Free HW device context
void CleanupHWDevice_dx11()
{
    g_pAdapter->Release();
}

CComPtr<ID3D11DeviceContext> GetHWDeviceContext()
{
    return g_pD3D11Ctx;
}

//
// Intel Media SDK memory allocator entrypoints....
//
static mfxStatus _simple_alloc(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    HRESULT hRes;

    // Determine surface format
    DXGI_FORMAT format;
    if (MFX_FOURCC_RGB4 == request->Info.FourCC)
        format = DXGI_FORMAT_B8G8R8A8_UNORM;
    else if (MFX_FOURCC_R16 == request->Info.FourCC)
        format = DXGI_FORMAT_R16_TYPELESS;
    else if(MFX_FOURCC_P8 == request->Info.FourCC || MFX_FOURCC_P8_TEXTURE == request->Info.FourCC)
        format = DXGI_FORMAT_P8;
    else
        format = DXGI_FORMAT_UNKNOWN;

    if (DXGI_FORMAT_UNKNOWN == format)
        return MFX_ERR_UNSUPPORTED;

    // Allocate custom container to keep texture and stage buffers for each surface
    // Container also stores the intended read and/or write operation.
    CustomMemId** mids = new CustomMemId *[request->NumFrameSuggested];
    if (!mids) return MFX_ERR_MEMORY_ALLOC;
    for (int i=0; i<request->NumFrameSuggested; i++) {
        mids[i] = new CustomMemId;
        memset(mids[i], 0, sizeof(CustomMemId));
        mids[i]->rw = request->Type & 0xF000; // Set intended read/write operation
    }

    request->Type = request->Type & 0x0FFF;

    {
        D3D11_TEXTURE2D_DESC desc = {0};

        desc.Width              = request->Info.Width;
        desc.Height             = request->Info.Height;
        desc.MipLevels          = 1;
        desc.ArraySize          = 1; // number of subresources is 1 in this case
        desc.Format             = format;
        desc.SampleDesc.Count   = 1;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_DECODER;
        desc.MiscFlags          = 0;
        //desc.MiscFlags            = D3D11_RESOURCE_MISC_SHARED;

        if ( (MFX_MEMTYPE_FROM_VPPIN & request->Type) &&
             (DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format) )
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        }

        if ( (MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
             (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type))
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        }

        if( DXGI_FORMAT_P8 == desc.Format )
            desc.BindFlags = 0;

        ID3D11Texture2D* pTexture2D;

        // Create surface textures
        for (size_t i = 0; i < request->NumFrameSuggested; i++)
        {
            hRes = g_pD3D11Device->CreateTexture2D(&desc, NULL, &pTexture2D);

            if (FAILED(hRes))
                return MFX_ERR_MEMORY_ALLOC;

            mids[i]->memId = pTexture2D;
        }

        desc.ArraySize      = 1;
        desc.Usage          = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;// | D3D11_CPU_ACCESS_WRITE;
        desc.BindFlags      = 0;
        desc.MiscFlags      = 0;
        //desc.MiscFlags        = D3D11_RESOURCE_MISC_SHARED;

        // Create surface staging textures
        for (size_t i = 0; i < request->NumFrameSuggested; i++)
        {
            hRes = g_pD3D11Device->CreateTexture2D(&desc, NULL, &pTexture2D);

            if (FAILED(hRes))
                return MFX_ERR_MEMORY_ALLOC;

            mids[i]->memIdStage = pTexture2D;
        }
    }


    response->mids = (mfxMemId*)mids;
    response->NumFrameActual = request->NumFrameSuggested;

    return MFX_ERR_NONE;
}

mfxStatus simple_alloc_dx11(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts = MFX_ERR_NONE;

    if(request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
        return MFX_ERR_UNSUPPORTED;

    sts = _simple_alloc(request, response);

    if (MFX_ERR_NONE == sts)
        allocResponses_dx11[response->mids] = pthis;

    return sts;
}

mfxStatus simple_lock_dx11(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    pthis; // To suppress warning for this unused parameter

    HRESULT hRes = S_OK;

    D3D11_TEXTURE2D_DESC        desc = {0};
    D3D11_MAPPED_SUBRESOURCE    lockedRect = {0};

    CustomMemId*        memId       = (CustomMemId*)mid;
    ID3D11Texture2D*    pSurface    = (ID3D11Texture2D *)memId->memId;
    ID3D11Texture2D*    pStage      = (ID3D11Texture2D *)memId->memIdStage;

    D3D11_MAP   mapType  = D3D11_MAP_READ;
    UINT        mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;

    if(NULL == pStage)
    {
        hRes = g_pD3D11Ctx->Map(pSurface, 0, mapType, mapFlags, &lockedRect);
        desc.Format = DXGI_FORMAT_P8;
    }
    else
    {
        pSurface->GetDesc(&desc);

        // copy data only in case of user wants o read from stored surface
        if(memId->rw & WILL_READ)
            g_pD3D11Ctx->CopySubresourceRegion(pStage, 0, 0, 0, 0, pSurface, 0, NULL);

        do
        {
            hRes = g_pD3D11Ctx->Map(pStage, 0, mapType, mapFlags, &lockedRect);
            if (S_OK != hRes && DXGI_ERROR_WAS_STILL_DRAWING != hRes)
                return MFX_ERR_LOCK_MEMORY;
        }
        while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);
    }

    if (FAILED(hRes))
        return MFX_ERR_LOCK_MEMORY;

    switch (desc.Format)
    {
        case DXGI_FORMAT_B8G8R8A8_UNORM :
            ptr->Pitch = (mfxU16)lockedRect.RowPitch;
            ptr->B = (mfxU8 *)lockedRect.pData;
            ptr->G = ptr->B + 1;
            ptr->R = ptr->B + 2;
            ptr->A = ptr->B + 3;
            break;
        case DXGI_FORMAT_P8 :
            ptr->Pitch = (mfxU16)lockedRect.RowPitch;
            ptr->Y = (mfxU8 *)lockedRect.pData;
            ptr->U = 0;
            ptr->V = 0;
            break;
        case DXGI_FORMAT_R16_TYPELESS :
            ptr->Pitch = (mfxU16)lockedRect.RowPitch;
            ptr->Y16 = (mfxU16 *)lockedRect.pData;
            ptr->U16 = 0;
            ptr->V16 = 0;
            break;
        default:
            return MFX_ERR_LOCK_MEMORY;
    }

    return MFX_ERR_NONE;
}

mfxStatus simple_unlock_dx11(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    pthis; // To suppress warning for this unused parameter

    CustomMemId*        memId       = (CustomMemId*)mid;
    ID3D11Texture2D*    pSurface    = (ID3D11Texture2D *)memId->memId;
    ID3D11Texture2D*    pStage      = (ID3D11Texture2D *)memId->memIdStage;

    if (NULL == pStage)
    {
        g_pD3D11Ctx->Unmap(pSurface, 0);
    }
    else
    {
        g_pD3D11Ctx->Unmap(pStage, 0);
        // copy data only in case of user wants to write to stored surface
        if(memId->rw & WILL_WRITE)
            g_pD3D11Ctx->CopySubresourceRegion(pSurface, 0, 0, 0, 0, pStage, 0, NULL);
    }

    if (ptr)
    {
        ptr->Pitch=0;
        ptr->U=ptr->V=ptr->Y=0;
        ptr->A=ptr->R=ptr->G=ptr->B=0;
    }

    return MFX_ERR_NONE;
}

mfxStatus simple_gethdl_dx11(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    pthis; // To suppress warning for this unused parameter

    if (NULL == handle)
        return MFX_ERR_INVALID_HANDLE;

    mfxHDLPair*     pPair = (mfxHDLPair*)handle;
    CustomMemId*    memId = (CustomMemId*)mid;

    pPair->first  = memId->memId; // surface texture
    pPair->second = 0;

    return MFX_ERR_NONE;
}


static mfxStatus _simple_free(mfxFrameAllocResponse *response)
{
    if (response->mids)
    {
        for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        {
            if (response->mids[i])
            {
                CustomMemId*        mid         = (CustomMemId*)response->mids[i];
                ID3D11Texture2D*    pSurface    = (ID3D11Texture2D *)mid->memId;
                ID3D11Texture2D*    pStage      = (ID3D11Texture2D *)mid->memIdStage;

                if(pSurface)
                    pSurface->Release();
                if(pStage)
                    pStage->Release();

                delete mid;
            }
        }
    }

    delete [] response->mids;
    response->mids = 0;

    return MFX_ERR_NONE;
}

mfxStatus simple_free_dx11(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (NULL == response)
        return MFX_ERR_NULL_PTR;

    if(allocResponses_dx11.find(response->mids) == allocResponses_dx11.end())
    {
        // Decode free response handling
        if(--allocDecodeRefCount_dx11[pthis] == 0)
        {
            _simple_free(response);
            allocDecodeResponses_dx11.erase(pthis);
            allocDecodeRefCount_dx11.erase(pthis);
        }
    }
    else
    {
        // Encode and VPP free response handling
        allocResponses_dx11.erase(response->mids);
        _simple_free(response);
    }

    return MFX_ERR_NONE;
}
