// Copyright (c) 2007-2020 Intel Corporation
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

#include "mfxvideo++int.h"
#include "libmfx_allocator_d3d11.h"

#include "ippcore.h"
#include "ipps.h"

#include "mfx_utils.h"
#include "mfx_common.h"
#include "mfxpcp.h"
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
#include "mfxpavp.h"
#endif

DXGI_FORMAT mfxDefaultAllocatorD3D11::MFXtoDXGI(mfxU32 format)
{
    switch (format)
    {
    case MFX_FOURCC_P010:
        return DXGI_FORMAT_P010;

    case MFX_FOURCC_NV12:
        return DXGI_FORMAT_NV12;

    case D3DFMT_YUY2:
        return DXGI_FORMAT_YUY2;

    case MFX_FOURCC_YUV400:
    case MFX_FOURCC_YUV411:
    case MFX_FOURCC_IMC3:
    case MFX_FOURCC_YUV422H:
    case MFX_FOURCC_YUV422V:
    case MFX_FOURCC_YUV444:
    case MFX_FOURCC_RGBP:
    case MFX_FOURCC_YV12:
        return DXGI_FORMAT_420_OPAQUE;

    case MFX_FOURCC_RGB4:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case MFX_FOURCC_BGR4:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case MFX_FOURCC_P8:
    case MFX_FOURCC_P8_TEXTURE:
        return DXGI_FORMAT_P8;

    case MFX_FOURCC_AYUV:
    case DXGI_FORMAT_AYUV:
        return DXGI_FORMAT_AYUV;
    case MFX_FOURCC_R16:
    case MFX_FOURCC_R16_BGGR:
    case MFX_FOURCC_R16_RGGB:
    case MFX_FOURCC_R16_GRBG:
    case MFX_FOURCC_R16_GBRG:
        return DXGI_FORMAT_R16_TYPELESS;
    case MFX_FOURCC_ARGB16:
    case MFX_FOURCC_ABGR16:
        return DXGI_FORMAT_R16G16B16A16_UNORM;
    case MFX_FOURCC_A2RGB10:
        return DXGI_FORMAT_R10G10B10A2_UNORM;

#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
        return DXGI_FORMAT_Y210;
    case MFX_FOURCC_Y410:
        return DXGI_FORMAT_Y410;
#endif

#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_P016:
        return DXGI_FORMAT_P016;
    case MFX_FOURCC_Y216:
        return DXGI_FORMAT_Y216;
    case MFX_FOURCC_Y416:
        return DXGI_FORMAT_Y416;
#endif
    }
    return DXGI_FORMAT_UNKNOWN;

} // mfxDefaultAllocatorD3D11::MFXtoDXGI(mfxU32 format)

static inline bool check_supported_fourcc(mfxU32 fourcc)
{
    // only NV12 and D3DFMT_P8 buffers are supported by HW
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_IMC3:
    case MFX_FOURCC_YUV400:
    case MFX_FOURCC_YUV411:
    case MFX_FOURCC_YUV422H:
    case MFX_FOURCC_YUV422V:
    case MFX_FOURCC_YUV444:
    case MFX_FOURCC_RGBP:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_BGR4:
    case MFX_FOURCC_P8:
    case MFX_FOURCC_P8_TEXTURE:
    case DXGI_FORMAT_AYUV:
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_R16_RGGB:
    case MFX_FOURCC_R16_BGGR:
    case MFX_FOURCC_R16_GBRG:
    case MFX_FOURCC_R16_GRBG:
    case MFX_FOURCC_ARGB16:
    case MFX_FOURCC_ABGR16:
    case MFX_FOURCC_A2RGB10:
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y410:
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_P016:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_Y416:
#endif
        return true;
    default:
        return false;
    }
}

// D3D11 surface working
mfxStatus mfxDefaultAllocatorD3D11::AllocFramesHW(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    MFX_CHECK_HDL(pthis);
    MFX_CHECK(check_supported_fourcc(request->Info.FourCC), MFX_ERR_UNSUPPORTED);

    mfxU16 numAllocated, maxNumFrames;
    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    pSelf->isBufferKeeper = false;
    pSelf->m_StagingSrfPool = NULL;

    // frames were allocated
    // return existent frames
    if (pSelf->NumFrames)
    {
        if (request->NumFrameSuggested > pSelf->NumFrames)
            return MFX_ERR_MEMORY_ALLOC;
        else
        {
            response->mids = &pSelf->m_frameHandles[0];
            return MFX_ERR_NONE;
        }

    }

    UINT    width = request->Info.Width;
    UINT    height = request->Info.Height;

    HRESULT hr = S_OK;

    maxNumFrames = request->NumFrameSuggested;

    //Create Texture
    D3D11_TEXTURE2D_DESC Desc = {0};
    Desc.Width = width;
    Desc.Height =  height;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = MFXtoDXGI(request->Info.FourCC);
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;

    if((request->Type&MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET) && (request->Type & MFX_MEMTYPE_INTERNAL_FRAME))
    {
        Desc.BindFlags = D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER;
    }
    else
        Desc.BindFlags = D3D11_BIND_DECODER;


    // P8 with 0
    if(request->Info.FourCC == MFX_FOURCC_P8)
    {
        pSelf->m_NumSurface = 1;

        D3D11_BUFFER_DESC desc = { 0 };

        desc.ByteWidth           = request->Info.Width * request->Info.Height;
        desc.Usage               = D3D11_USAGE_STAGING;
        desc.BindFlags           = 0;
        desc.CPUAccessFlags      = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags           = 0;
        desc.StructureByteStride = 0;

        ID3D11Buffer * buffer = 0;
        hr = pSelf->m_pD11Device->CreateBuffer(&desc, 0, &buffer);
        if (FAILED(hr))
            return MFX_ERR_MEMORY_ALLOC;
        Desc.BindFlags = 0;
        pSelf->m_SrfPool.push_back(reinterpret_cast<ID3D11Texture2D *>(buffer));
        pSelf->isBufferKeeper = true;
        {
            pSelf->m_frameHandles.resize(maxNumFrames);
            for (numAllocated = 0; numAllocated < maxNumFrames; numAllocated++)
            {
                size_t id = (size_t)(numAllocated + 1);
                pSelf->m_frameHandles[numAllocated] = (mfxHDL)id ;
            }
            response->mids = &pSelf->m_frameHandles[0];
            response->NumFrameActual = maxNumFrames;
            pSelf->NumFrames = maxNumFrames;
        }
    }
    else
    {

        if ( (MFX_MEMTYPE_FROM_VPPIN & request->Type) && (DXGI_FORMAT_YUY2 == Desc.Format) ||
             (DXGI_FORMAT_B8G8R8A8_UNORM == Desc.Format) ||
             (DXGI_FORMAT_R8G8B8A8_UNORM == Desc.Format) ||
             (DXGI_FORMAT_R10G10B10A2_UNORM == Desc.Format) )
        {
            Desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (Desc.ArraySize > 2)
                return MFX_ERR_MEMORY_ALLOC;
        }

        if ( (MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
             (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type))
        {
            Desc.BindFlags = D3D11_BIND_RENDER_TARGET;

            // change request->Type
            // only MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET is allowed for VPP OUT
            if (!(MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type))
            {
                request->Type = request->Type & 0xFF0F; //reset old flags
                request->Type = request->Type | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
            }

            if (Desc.ArraySize > 2)
                return MFX_ERR_MEMORY_ALLOC;
        }
        pSelf->m_NumSurface = maxNumFrames;
        pSelf->m_SrfPool.resize(maxNumFrames);

        if(request->Type & MFX_MEMTYPE_SHARED_RESOURCE)
        {
            Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            Desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        }

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        if (request->Type & MFX_MEMTYPE_PROTECTED)
        {
            Desc.MiscFlags |= D3D11_RESOURCE_MISC_HW_PROTECTED;
        }
#endif //!OPEN_SOURCE

        // d3d11 wo
        if( DXGI_FORMAT_P8 == Desc.Format )
        {
            Desc.BindFlags = 0;
        }
        for (mfxU32 i = 0; i < maxNumFrames; i++)
        {
            if ( DXGI_FORMAT_R16_TYPELESS == Desc.Format)
            {
                // R16 Bayer requires having special resource extension reflecting
                // real Bayer format
                Desc.MipLevels = 1;
                Desc.ArraySize = 1;
                Desc.SampleDesc.Count   = 1;
                Desc.SampleDesc.Quality = 0;
                Desc.Usage     = D3D11_USAGE_DEFAULT;
                Desc.BindFlags = 0;
                Desc.CPUAccessFlags = 0;// = D3D11_CPU_ACCESS_WRITE;
                Desc.MiscFlags = 0;
                RESOURCE_EXTENSION extnDesc;
                ZeroMemory( &extnDesc, sizeof(RESOURCE_EXTENSION) );
                static_assert (sizeof(RESOURCE_EXTENSION_KEY) <= sizeof(extnDesc.Key), "");
                std::copy(std::begin(RESOURCE_EXTENSION_KEY), std::end(RESOURCE_EXTENSION_KEY), extnDesc.Key);
                extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
                extnDesc.Type    = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
                extnDesc.Data[0] = BayerFourCC2FormatFlag(request->Info.FourCC);
                hr = SetResourceExtension(pSelf->m_pD11Device, &extnDesc);
            }

            hr = pSelf->m_pD11Device->CreateTexture2D(&Desc, NULL, &pSelf->m_SrfPool[i]);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_MEMORY_ALLOC);

            if (Desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) {
                CComPtr<IDXGIKeyedMutex> mutex;
                hr = pSelf->m_SrfPool[i]->QueryInterface(IID_PPV_ARGS(&mutex));
                MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

                hr = mutex->AcquireSync(0, 1000);
                MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
            }
        }

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        if (request->Type & MFX_MEMTYPE_PROTECTED)
        {
            Desc.MiscFlags &= ~D3D11_RESOURCE_MISC_HW_PROTECTED;
        }
#endif //!OPEN_SOURCE

        // Create Staging buffers for fast coping (do not need for 420 opaque)
        if(Desc.Format != DXGI_FORMAT_420_OPAQUE)
        {
            Desc.ArraySize = 1;
            Desc.Usage = D3D11_USAGE_STAGING;
            Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            Desc.BindFlags = 0;
            Desc.MiscFlags = 0;

            hr = pSelf->m_pD11Device->CreateTexture2D(&Desc, NULL, &pSelf->m_StagingSrfPool);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_MEMORY_ALLOC);
        }

        // create mapping table
        pSelf->m_frameHandles.resize(maxNumFrames);
        for (numAllocated = 0; numAllocated < maxNumFrames; numAllocated++)
        {
            size_t id = (size_t)(numAllocated + 1);
            pSelf->m_frameHandles[numAllocated] = (mfxHDL)id ;
        }
        response->mids = &pSelf->m_frameHandles[0];
        response->NumFrameActual = maxNumFrames;
        pSelf->NumFrames = maxNumFrames;

        // check the number of allocated frames
        if (numAllocated < request->NumFrameMin)
        {
            FreeFramesHW(pthis, response);
            return MFX_ERR_MEMORY_ALLOC;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorD3D11::SetFrameData(const D3D11_TEXTURE2D_DESC &Desc, const D3D11_MAPPED_SUBRESOURCE &LockedRect, mfxFrameData &frame_data)
{
    clear_frame_data(frame_data);

    // not sure about commented formats
    switch (Desc.Format)
    {
    case DXGI_FORMAT_P010:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case DXGI_FORMAT_P016:
#endif
        frame_data.Y = (mfxU8 *)LockedRect.pData;
        frame_data.U = (mfxU8 *)LockedRect.pData + Desc.Height * LockedRect.RowPitch;
        frame_data.V = frame_data.U + 2;
        break;
    case DXGI_FORMAT_NV12:
        frame_data.Y = (mfxU8 *)LockedRect.pData;
        frame_data.U = (mfxU8 *)LockedRect.pData + Desc.Height * LockedRect.RowPitch;
        frame_data.V = frame_data.U + 1;
        break;
    case DXGI_FORMAT_420_OPAQUE: // YV12 ????
        frame_data.Y = (mfxU8 *)LockedRect.pData;
        frame_data.V = frame_data.Y + Desc.Height * LockedRect.RowPitch;
        frame_data.U = frame_data.V + (Desc.Height * LockedRect.RowPitch) / 4;
        break;
    case DXGI_FORMAT_YUY2:
        frame_data.Y = (mfxU8 *)LockedRect.pData;
        frame_data.U = frame_data.Y + 1;
        frame_data.V = frame_data.Y + 3;
        break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        frame_data.B = (mfxU8 *)LockedRect.pData;
        frame_data.G = frame_data.B + 1;
        frame_data.R = frame_data.B + 2;
        frame_data.A = frame_data.B + 3;
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        frame_data.R = (mfxU8 *)LockedRect.pData;
        frame_data.G = frame_data.R + 1;
        frame_data.B = frame_data.R + 2;
        frame_data.A = frame_data.R + 3;
        break;
    case DXGI_FORMAT_AYUV:
        frame_data.B = (mfxU8 *)LockedRect.pData;
        frame_data.G = frame_data.B + 1;
        frame_data.R = frame_data.B + 2;
        frame_data.A = frame_data.B + 3;
        break;
    case DXGI_FORMAT_P8:
        frame_data.Y = (mfxU8 *)LockedRect.pData;
        break;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        frame_data.R = frame_data.G = frame_data.B = frame_data.A = (mfxU8 *)LockedRect.pData;
        break;

#if (MFX_VERSION >= 1027)
    case DXGI_FORMAT_Y410:
        frame_data.Y410 = (mfxY410 *)LockedRect.pData;
        break;
    case DXGI_FORMAT_Y210:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case DXGI_FORMAT_Y216:
#endif
        frame_data.Y16 = (mfxU16*)LockedRect.pData;
        frame_data.U16 = frame_data.Y16 + 1;
        frame_data.V16 = frame_data.Y16 + 3;
        break;
#endif

#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case DXGI_FORMAT_Y416:
        frame_data.U16 = (mfxU16*)LockedRect.pData;
        frame_data.Y16 = frame_data.U16 + 1;
        frame_data.V16 = frame_data.Y16 + 1;
        frame_data.A   = (mfxU8 *)(frame_data.V16 + 1);
        break;
#endif

    default:
        MFX_RETURN(MFX_ERR_LOCK_MEMORY);
    }

    frame_data.PitchHigh = mfxU16(LockedRect.RowPitch >> 16);
    frame_data.PitchLow  = mfxU16(LockedRect.RowPitch & 0xffff);

    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorD3D11::LockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    HRESULT hr = S_OK;
    // TBD
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    size_t index =  (size_t)mid - 1;

    D3D11_TEXTURE2D_DESC Desc = {0};

    D3D11_MAPPED_SUBRESOURCE LockedRect = {0};
    D3D11_MAP MapType = D3D11_MAP_READ;
    UINT MapFlags = 0;// D3D11_MAP_FLAG_DO_NOT_WAIT; //!!!!!!WA synchronization issue on Win10, requires to return back after fix, can affect performance negatively
    
    // need to copy surface into staging surface then map
    ID3D11Texture2D *pStagingSurface = pSelf->m_StagingSrfPool;

    if (!pSelf->isBufferKeeper)
    {
        pSelf->m_SrfPool[index]->GetDesc(&Desc);
        pSelf->m_pD11DeviceContext->CopySubresourceRegion(pStagingSurface, 0, 0, 0, 0, pSelf->m_SrfPool[index], 0, NULL);
    }
    else
    {
        pStagingSurface = pSelf->m_SrfPool[index];
        Desc.Format = DXGI_FORMAT_P8;
    }

    do
    {
        hr = pSelf->m_pD11DeviceContext->Map(pStagingSurface, 0, MapType, MapFlags, &LockedRect);
    }
    while (DXGI_ERROR_WAS_STILL_DRAWING == hr);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    return mfxDefaultAllocatorD3D11::SetFrameData(Desc, LockedRect, *ptr);
}
mfxStatus mfxDefaultAllocatorD3D11::GetHDLHW(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    if (0 == mid)
        return MFX_ERR_INVALID_HANDLE;


    if (0 == handle)
        return MFX_ERR_INVALID_HANDLE;

    mfxHDLPair *pPair =  (mfxHDLPair*)handle;
    size_t index      =  (size_t)mid - 1;

    if (index >= pSelf->m_SrfPool.size())
        return MFX_ERR_INVALID_HANDLE;

    // resource pool handle
    pPair->first = pSelf->m_SrfPool[index];
    // index of pool
    pPair->second = (mfxHDL *)0;

    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorD3D11::UnlockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    size_t index =  (size_t)mid - 1;
    ID3D11Texture2D    *pStagingSurface  = pSelf->m_StagingSrfPool;
    if (!pSelf->isBufferKeeper)
    {
        pSelf->m_pD11DeviceContext->Unmap(pStagingSurface, 0);
        pSelf->m_pD11DeviceContext->CopySubresourceRegion(pSelf->m_SrfPool[index], 0, 0, 0, 0, pStagingSurface, 0, 0);
    }
    else
    {
        pSelf->m_pD11DeviceContext->Unmap(pSelf->m_SrfPool.back(), 0);
    }

    if (ptr)
    {
        ptr->PitchHigh=0;
        ptr->PitchLow=0;
        ptr->U=ptr->V=ptr->Y=0;
        ptr->A=ptr->R=ptr->G=ptr->B=0;
    }

    return MFX_ERR_NONE;
}
mfxStatus mfxDefaultAllocatorD3D11::FreeFramesHW(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;

    for (mfxU32 i = 0; i < pSelf->m_NumSurface; i++)
        SAFE_RELEASE(pSelf->m_SrfPool[i]);

    SAFE_RELEASE(pSelf->m_StagingSrfPool);

    response;

    // TBD
    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorD3D11::ReallocFrameHW(mfxHDL pthis, const mfxMemId mid, const mfxFrameInfo *info)
{
    MFX_CHECK(info,MFX_ERR_NULL_PTR);
    MFX_CHECK(pthis, MFX_ERR_NULL_PTR);
    MFX_CHECK(mid, MFX_ERR_NULL_PTR);

    HRESULT hRes;

    DXGI_FORMAT colorFormat = MFXtoDXGI(info->FourCC);

    if (DXGI_FORMAT_UNKNOWN == colorFormat)
    {
        char *cfcc = (char *)&info->FourCC;
        vm_string_printf(VM_STRING("Unknown format: %c%c%c%c\n"), cfcc[0], cfcc[1], cfcc[2], cfcc[3]);
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    if (!pSelf)
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);

    if (info->FourCC == MFX_FOURCC_P8)
    {
        /*D3D11_BUFFER_DESC desc = { 0 };

        desc.ByteWidth           = info->Width * info->Height;
        desc.Usage               = D3D11_USAGE_STAGING;
        desc.BindFlags           = 0;
        desc.CPUAccessFlags      = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags           = 0;
        desc.StructureByteStride = 0;

        ID3D11Buffer * buffer = 0;
        hRes = m_initParams.pDevice->CreateBuffer(&desc, 0, &buffer);
        if (FAILED(hRes))
            return MFX_ERR_MEMORY_ALLOC;

        newTexture.textures.push_back(reinterpret_cast<ID3D11Texture2D *>(buffer));*/
    }
    else
    {
        D3D11_TEXTURE2D_DESC desc = { 0 };

        desc.Width = info->Width;
        desc.Height = info->Height;
        desc.MipLevels = 1;
        //number of subresources is 1 in case of not single texture
        desc.ArraySize = 1;
        desc.Format = MFXtoDXGI(info->FourCC);
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;

        desc.BindFlags = D3D11_BIND_DECODER;

        /*if ( (MFX_MEMTYPE_FROM_VPPIN & request->Type) && (DXGI_FORMAT_YUY2 == desc.Format) ||
             (DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format) ||
             (DXGI_FORMAT_R10G10B10A2_UNORM == desc.Format) )
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (desc.ArraySize > 2)
                return MFX_ERR_MEMORY_ALLOC;
        }

        if ( (MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
             (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type))
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (desc.ArraySize > 2)
                return MFX_ERR_MEMORY_ALLOC;
        }*/

        if (DXGI_FORMAT_P8 == desc.Format)
        {
            desc.BindFlags = 0;
        }
        //release surface that need to reallocate
        size_t index = (size_t)mid - 1;
        if (pSelf->m_SrfPool.size() <= index)
            MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
        SAFE_RELEASE(pSelf->m_SrfPool[index]);
        if (pSelf->m_SrfPool[index] != NULL)
            MFX_RETURN(MFX_ERR_MEMORY_ALLOC);

        //reallocate released surface with new desc
        hRes = pSelf->m_pD11Device->CreateTexture2D(&desc, NULL, &pSelf->m_SrfPool[index]);
        if (FAILED(hRes))
        {
            vm_string_printf(VM_STRING("CreateTexture2D failed, hr = 0x%X\n"), hRes);
            MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
        }
    }
    return MFX_ERR_NONE;
}

mfxDefaultAllocatorD3D11::mfxWideHWFrameAllocator::mfxWideHWFrameAllocator(mfxU16 type,
                                                                           ID3D11Device *pD11Device,
                                                                           ID3D11DeviceContext  *pD11DeviceContext):mfxBaseWideFrameAllocator(type),
                                                                                                                    m_StagingSrfPool(0),
                                                                                                                    m_pD11Device(pD11Device),
                                                                                                                    m_pD11DeviceContext(pD11DeviceContext),
                                                                                                                    m_NumSurface(0),
                                                                                                                    isBufferKeeper(false)
{
    frameAllocator.Alloc =  &mfxDefaultAllocatorD3D11::AllocFramesHW;
    frameAllocator.Lock =   &mfxDefaultAllocatorD3D11::LockFrameHW;
    frameAllocator.GetHDL = &mfxDefaultAllocatorD3D11::GetHDLHW;
    frameAllocator.Unlock = &mfxDefaultAllocatorD3D11::UnlockFrameHW;
    frameAllocator.Free =   &mfxDefaultAllocatorD3D11::FreeFramesHW;
}

staging_texture* staging_adapter_d3d11_texture::get_staging_texture(d3d11_texture_wrapper* main_texture, const D3D11_TEXTURE2D_DESC& descr)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto it = m_textures.find(main_texture);
    bool contains_main_texture = it != std::end(m_textures);

    if (contains_main_texture && !it->second->m_acquired)
    {
        return it->second.get();
    }

    it = std::find_if(std::begin(m_textures), std::end(m_textures),
        [&descr](const std::pair <d3d11_texture_wrapper*, std::shared_ptr<staging_texture>>& texture)
                {
                    return !texture.second->m_acquired && texture.second->m_description == descr;
                });

    if (it != std::end(m_textures))
    {
        it->second->m_acquired = true;

        m_textures.insert({ main_texture, it->second });

        return it->second.get();
    }

    lock.unlock();
    // If no suitable cached surface found - allocate another one
    D3D11_TEXTURE2D_DESC Descr = descr;
    Descr.Usage          = D3D11_USAGE_STAGING;
    Descr.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    Descr.BindFlags      = 0;

    ID3D11Texture2D* texture;
    HRESULT hr = m_pD11Device->CreateTexture2D(&Descr, nullptr, &texture);
    MFX_CHECK_WITH_THROW(SUCCEEDED(hr), MFX_ERR_MEMORY_ALLOC, mfx::mfxStatus_exception(MFX_ERR_MEMORY_ALLOC));

    auto new_staging_texture = std::make_shared<staging_texture>(false, texture, descr, m_mutex);
    lock.lock();

    if (contains_main_texture)
    {
        m_textures[main_texture] = std::move(new_staging_texture);
    }
    else
    {
        m_textures.insert({ std::move(main_texture), std::move(new_staging_texture) });
    }

    m_textures[main_texture]->m_acquired = true;
    return m_textures[main_texture].get();
}

inline mfxStatus mfx_to_d3d11_flags(mfxU32 flags, D3D11_MAP& map_type, UINT& map_flags)
{
    switch (flags & 0xf)
    {
    case MFX_MAP_READ:
        map_type = D3D11_MAP_READ;
        break;
    case MFX_MAP_WRITE:
        map_type = D3D11_MAP_WRITE;
        break;
    case MFX_MAP_READ_WRITE:
        map_type = D3D11_MAP_READ_WRITE;
        break;
    default:
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    map_flags = (flags & MFX_MAP_NOWAIT) ? D3D11_MAP_FLAG_DO_NOT_WAIT : 0;

    return MFX_ERR_NONE;
}

d3d11_buffer_wrapper::d3d11_buffer_wrapper(const mfxFrameInfo &info, mfxHDL device)
    : d3d11_resource_wrapper(reinterpret_cast<ID3D11Device*>(device))
{
    mfxStatus sts = AllocBuffer(info);
    MFX_CHECK_WITH_THROW(MFX_SUCCEEDED(sts), MFX_ERR_MEMORY_ALLOC, mfx::mfxStatus_exception(MFX_ERR_MEMORY_ALLOC));
}

mfxStatus d3d11_buffer_wrapper::AllocBuffer(const mfxFrameInfo & info)
{
    D3D11_BUFFER_DESC desc = { mfxU32(info.Width * info.Height), D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0u, 0u };

    ID3D11Buffer* buffer;
    HRESULT hr = m_pD11Device->CreateBuffer(&desc, nullptr, &buffer);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_MEMORY_ALLOC);

    m_resource.Attach(buffer);

    return MFX_ERR_NONE;
}

mfxStatus d3d11_buffer_wrapper::Lock(mfxFrameData& frame_data, mfxU32 flags)
{
    MFX_CHECK(FrameAllocatorBase::CheckMemoryFlags(flags), MFX_ERR_LOCK_MEMORY);

    D3D11_MAP MapType;
    UINT MapFlags;

    MFX_SAFE_CALL(mfx_to_d3d11_flags(flags, MapType, MapFlags));

    CComPtr<ID3D11DeviceContext> pImmediateContext;
    m_pD11Device->GetImmediateContext(&pImmediateContext);

    HRESULT hr = pImmediateContext->Map(m_resource, 0, MapType, MapFlags, &m_LockedRect);
    MFX_CHECK(hr != DXGI_ERROR_WAS_STILL_DRAWING, MFX_ERR_RESOURCE_MAPPED);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_LOCK_MEMORY);

    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Format = DXGI_FORMAT_P8;
    return mfxDefaultAllocatorD3D11::SetFrameData(Desc, m_LockedRect, frame_data);
}

mfxStatus d3d11_buffer_wrapper::Unlock()
{
    CComPtr<ID3D11DeviceContext> pImmediateContext;
    m_pD11Device->GetImmediateContext(&pImmediateContext);

    pImmediateContext->Unmap(m_resource, 0);

    return MFX_ERR_NONE;
}

mfxStatus d3d11_buffer_wrapper::Realloc(const mfxFrameInfo & info)
{
    return AllocBuffer(info);
}

d3d11_texture_wrapper::d3d11_texture_wrapper(const mfxFrameInfo &info, mfxU16 type, staging_adapter_d3d11_texture & stg_adapter, mfxHDL device)
    : d3d11_resource_wrapper(reinterpret_cast<ID3D11Device*>(device))
    , m_staging_adapter(stg_adapter)
    , m_staging_surface(nullptr)
    , m_type(type)
{
    mfxStatus sts = AllocFrame(info);
    MFX_CHECK_WITH_THROW(MFX_SUCCEEDED(sts), MFX_ERR_MEMORY_ALLOC, mfx::mfxStatus_exception(MFX_ERR_MEMORY_ALLOC));
}

mfxStatus d3d11_texture_wrapper::AllocFrame(const mfxFrameInfo & info)
{
    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width                = info.Width;
    Desc.Height               = info.Height;
    Desc.MipLevels            = 1;
    Desc.ArraySize            = 1;
    Desc.Format               = mfxDefaultAllocatorD3D11::MFXtoDXGI(info.FourCC);
    Desc.SampleDesc.Count     = 1;
    Desc.Usage                = D3D11_USAGE_DEFAULT;
    Desc.BindFlags            = D3D11_BIND_DECODER;

    if ((m_type & MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET) && (m_type & MFX_MEMTYPE_INTERNAL_FRAME))
    {
        Desc.BindFlags |= D3D11_BIND_VIDEO_ENCODER;
    }

    if ((MFX_MEMTYPE_FROM_VPPIN & m_type) && (DXGI_FORMAT_YUY2 == Desc.Format)
     || (MFX_MEMTYPE_FROM_VPPOUT & m_type)
     || (DXGI_FORMAT_B8G8R8A8_UNORM    == Desc.Format)
     || (DXGI_FORMAT_R8G8B8A8_UNORM    == Desc.Format)
     || (DXGI_FORMAT_R10G10B10A2_UNORM == Desc.Format))
    {
        Desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    }

    if (m_type & MFX_MEMTYPE_SHARED_RESOURCE)
    {
        // 420 OPAQUE formats don't support shader binding, but we have to set those two following flags
        // in as many situations as possible to provide maximal compatibility for internally allocated and
        // exposed to user HW surfaces
        if (Desc.Format != DXGI_FORMAT_420_OPAQUE)
            Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

        Desc.MiscFlags  = D3D11_RESOURCE_MISC_SHARED;
    }

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (m_type & MFX_MEMTYPE_PROTECTED)
    {
        Desc.MiscFlags |= D3D11_RESOURCE_MISC_HW_PROTECTED;
    }
#endif

    // d3d11 wo
    if (DXGI_FORMAT_P8 == Desc.Format)
    {
        Desc.BindFlags = 0;
    }

    HRESULT hr;
    if (DXGI_FORMAT_R16_TYPELESS == Desc.Format)
    {
        // R16 Bayer requires having special resource extension reflecting
        // real Bayer format
        Desc.SampleDesc.Quality = 0;
        Desc.BindFlags          = 0;
        Desc.CPUAccessFlags     = 0;// = D3D11_CPU_ACCESS_WRITE;
        Desc.MiscFlags          = 0;

        RESOURCE_EXTENSION extnDesc = {};

        static_assert (sizeof(RESOURCE_EXTENSION_KEY) <= sizeof(extnDesc.Key), "ERROR: Array size mismatch between extnDesc.Key and RESOURCE_EXTENSION_KEY");
        std::copy(std::begin(RESOURCE_EXTENSION_KEY), std::end(RESOURCE_EXTENSION_KEY), extnDesc.Key);

        extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
        extnDesc.Type               = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
        extnDesc.Data[0]            = BayerFourCC2FormatFlag(info.FourCC);

        hr = SetResourceExtension(m_pD11Device, &extnDesc);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_MEMORY_ALLOC);
    }

    ID3D11Texture2D* texture;
    hr = m_pD11Device->CreateTexture2D(&Desc, nullptr, &texture);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_MEMORY_ALLOC);

    m_resource.Attach(texture);

    return MFX_ERR_NONE;
}


mfxStatus d3d11_texture_wrapper::Lock(mfxFrameData& frame_data, mfxU32 flags)
{
    MFX_CHECK(FrameAllocatorBase::CheckMemoryFlags(flags), MFX_ERR_LOCK_MEMORY);

    D3D11_TEXTURE2D_DESC Desc = {};
    reinterpret_cast<ID3D11Texture2D *>((ID3D11Resource*)m_resource)->GetDesc(&Desc);

    // TODO: not clear if we need this restriction
    MFX_CHECK(Desc.Format != DXGI_FORMAT_420_OPAQUE, MFX_ERR_LOCK_MEMORY);

    CComPtr<ID3D11DeviceContext> pImmediateContext;
    m_pD11Device->GetImmediateContext(&pImmediateContext);

    // In d3d11 mapping is performed on staging surfaces
    unique_ptr_staging_texture staging_surface(m_staging_adapter.get_staging_texture(this, Desc));

    // If read access requested need to copy actual pixel values to staging surface
    if (flags & MFX_MAP_READ)
    {
        pImmediateContext->CopySubresourceRegion(staging_surface->m_texture, 0, 0, 0, 0, m_resource, 0, nullptr);
    }

    D3D11_MAP MapType;
    UINT MapFlags;
    MFX_SAFE_CALL(mfx_to_d3d11_flags(flags, MapType, MapFlags));

    HRESULT hr = pImmediateContext->Map(staging_surface->m_texture, 0, MapType, MapFlags, &m_LockedRect);
    MFX_CHECK(hr != DXGI_ERROR_WAS_STILL_DRAWING, MFX_ERR_RESOURCE_MAPPED);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_LOCK_MEMORY);

    m_staging_surface = std::move(staging_surface);

    mfxStatus sts = mfxDefaultAllocatorD3D11::SetFrameData(Desc, m_LockedRect, frame_data);

    if (sts != MFX_ERR_NONE)
    {
        // If Lock fails, all resources should be released
        MFX_STS_TRACE(Unlock());

        return sts;
    }

    // If user requested write access need to copy back from staging surface on unmap
    m_was_locked_for_write = flags & MFX_MAP_WRITE;

    return MFX_ERR_NONE;
}

mfxStatus d3d11_texture_wrapper::Unlock()
{
    // Here staging texture should be already acquired
    MFX_CHECK(m_staging_surface, MFX_ERR_UNKNOWN);

    CComPtr<ID3D11DeviceContext> pImmediateContext;
    m_pD11Device->GetImmediateContext(&pImmediateContext);

    pImmediateContext->Unmap(m_staging_surface->m_texture, 0);

    // If write access requested need to copy actual pixel values from staging surface
    if (m_was_locked_for_write)
    {
        pImmediateContext->CopySubresourceRegion(m_resource, 0, 0, 0, 0, m_staging_surface->m_texture, 0, nullptr);

        m_was_locked_for_write = false;
    }

    m_staging_surface.reset();
    m_LockedRect = {};

    return MFX_ERR_NONE;
}

mfxStatus d3d11_texture_wrapper::Realloc(const mfxFrameInfo & info)
{
    MFX_SAFE_CALL(AllocFrame(info));

    D3D11_TEXTURE2D_DESC Desc = {};
    reinterpret_cast<ID3D11Texture2D *>((ID3D11Resource*)m_resource)->GetDesc(&Desc);

    m_staging_adapter.UpdateCache(this, Desc);

    return MFX_ERR_NONE;
}

mfxFrameSurface1_hw_d3d11::mfxFrameSurface1_hw_d3d11(const mfxFrameInfo & info, mfxU16 type, mfxMemId mid, staging_adapter_d3d11_texture & stg_adapter, mfxHDL device, mfxU32, FrameAllocatorBase& allocator)
    : RWAcessSurface(info, type, mid, allocator)
    , m_pD11Device(reinterpret_cast<ID3D11Device*>(device))
    , m_staging_adapter(stg_adapter)

{
    MFX_CHECK_WITH_THROW(check_supported_fourcc(info.FourCC), MFX_ERR_UNSUPPORTED, mfx::mfxStatus_exception(MFX_ERR_UNSUPPORTED));
    MFX_CHECK_WITH_THROW(!(type & MFX_MEMTYPE_SYSTEM_MEMORY), MFX_ERR_UNSUPPORTED, mfx::mfxStatus_exception(MFX_ERR_UNSUPPORTED));

    if (info.FourCC == MFX_FOURCC_P8)
    {
        m_resource_wrapper.reset(new d3d11_buffer_wrapper(info, m_pD11Device));
    }
    else
    {
        m_resource_wrapper.reset(new d3d11_texture_wrapper(info, type, m_staging_adapter, m_pD11Device));
    }
}

mfxStatus mfxFrameSurface1_hw_d3d11::Lock(mfxU32 flags)
{

    MFX_CHECK(FrameAllocatorBase::CheckMemoryFlags(flags), MFX_ERR_LOCK_MEMORY);

    std::unique_lock<std::mutex> guard(m_mutex);

    mfxStatus sts = LockRW(guard, flags & MFX_MAP_WRITE, flags & MFX_MAP_NOWAIT);
    MFX_CHECK_STS(sts);

    auto Unlock = [](RWAcessSurface* s){ s->UnlockRW(); };

    // Scope guard to decrease locked count if real lock fails
    std::unique_ptr<RWAcessSurface, decltype(Unlock)> scoped_lock(this, Unlock);

    if (NumReaders() < 2)
    {
        // First reader or unique writer has just acquired resource
        sts = m_resource_wrapper->Lock(m_internal_surface.Data, flags);
        MFX_CHECK_STS(sts);
    }

    // No error, remove guard without decreasing locked counter
    scoped_lock.release();

    return MFX_ERR_NONE;
}

mfxStatus mfxFrameSurface1_hw_d3d11::Unlock()
{
    std::unique_lock<std::mutex> guard(m_mutex);

    MFX_SAFE_CALL(UnlockRW());

    if (NumReaders() == 0) // So it was 1 before UnlockRW
    {
        MFX_SAFE_CALL(m_resource_wrapper->Unlock());

        clear_frame_data(m_internal_surface.Data);
    }

    return MFX_ERR_NONE;
}

mfxStatus mfxFrameSurface1_hw_d3d11::GetHDL(mfxHDL& handle) const
{
    std::shared_lock<std::shared_timed_mutex> guard(m_hdl_mutex);

    mfxHDLPair &pair = reinterpret_cast<mfxHDLPair&>(handle);

    // resource pool handle
    pair.first  = m_resource_wrapper->GetHandle();
    // index of pool
    pair.second = mfxHDL(0);

    return MFX_ERR_NONE;
}

mfxStatus mfxFrameSurface1_hw_d3d11::Realloc(const mfxFrameInfo & info)
{
    std::lock_guard<std::mutex>              guard(m_mutex);
    std::lock_guard<std::shared_timed_mutex> guard_hdl(m_hdl_mutex);

    MFX_CHECK(!Locked(), MFX_ERR_LOCK_MEMORY);

    m_internal_surface.Info = info;

    return m_resource_wrapper->Realloc(info);
}

std::pair<mfxHDL, mfxResourceType> mfxFrameSurface1_hw_d3d11::GetNativeHandle() const
{
    std::shared_lock<std::shared_timed_mutex> guard(m_hdl_mutex);

    return { m_resource_wrapper->GetHandle(), MFX_RESOURCE_DX11_TEXTURE };
}

std::pair<mfxHDL, mfxHandleType> mfxFrameSurface1_hw_d3d11::GetDeviceHandle() const
{
    return { m_pD11Device, MFX_HANDLE_D3D11_DEVICE };
}


#endif

