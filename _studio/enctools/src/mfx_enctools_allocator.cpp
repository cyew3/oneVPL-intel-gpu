// Copyright (c) 2008-2021 Intel Corporation
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

#include <assert.h>
#include <algorithm>
#include <functional>
#include "mfx_enctools.h"

MFXFrameAllocator::MFXFrameAllocator()
{
    pthis = this;
    Alloc = Alloc_;
    Lock  = Lock_;
    Free  = Free_;
    Unlock = Unlock_;
    GetHDL = GetHDL_;
}

MFXFrameAllocator::~MFXFrameAllocator()
{
}

mfxStatus MFXFrameAllocator::Alloc_(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.AllocFrames(request, response);
}

mfxStatus MFXFrameAllocator::Lock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.LockFrame(mid, ptr);
}

mfxStatus MFXFrameAllocator::Unlock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.UnlockFrame(mid, ptr);
}

mfxStatus MFXFrameAllocator::Free_(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.FreeFrames(response);
}

mfxStatus MFXFrameAllocator::GetHDL_(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.GetFrameHDL(mid, handle);
}

BaseFrameAllocator::BaseFrameAllocator()
{
}

BaseFrameAllocator::~BaseFrameAllocator()
{
}

mfxStatus BaseFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    if (0 == request)
        return MFX_ERR_NULL_PTR;

    // check that Media SDK component is specified in request
    if ((request->Type & MEMTYPE_FROM_MASK) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus BaseFrameAllocator::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{

    if (0 == request || 0 == response || 0 == request->NumFrameSuggested)
        return MFX_ERR_MEMORY_ALLOC;

    if (MFX_ERR_NONE != CheckRequestType(request))
        return MFX_ERR_UNSUPPORTED;

    mfxStatus sts = MFX_ERR_NONE;

    if ( (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) && (request->Type & MFX_MEMTYPE_FROM_DECODE) )
    {
        // external decoder allocations
        std::list<UniqueResponse>::iterator it =
            std::find_if( m_ExtResponses.begin()
                        , m_ExtResponses.end()
                        , UniqueResponse (*response, request->Info.CropW, request->Info.CropH, 0));

        if (it != m_ExtResponses.end())
        {
            // check if enough frames were allocated
            if (request->NumFrameMin > it->NumFrameActual)
                return MFX_ERR_MEMORY_ALLOC;

            it->m_refCount++;
            // return existing response
            *response = (mfxFrameAllocResponse&)*it;
        }
        else
        {
            sts = AllocImpl(request, response);
            if (sts == MFX_ERR_NONE)
            {
                m_ExtResponses.push_back(UniqueResponse(*response, request->Info.CropW, request->Info.CropH, request->Type & MEMTYPE_FROM_MASK));
            }
        }
    }
    else
    {
        // internal allocations

        // reserve space before allocation to avoid memory leak
        std::list<mfxFrameAllocResponse> tmp(1, mfxFrameAllocResponse(), m_responses.get_allocator());

        sts = AllocImpl(request, response);
        if (sts == MFX_ERR_NONE)
        {
            m_responses.splice(m_responses.end(), tmp);
            m_responses.back() = *response;
        }
    }

    return sts;
}

mfxStatus BaseFrameAllocator::FreeFrames(mfxFrameAllocResponse *response)
{
    if (response == 0)
        return MFX_ERR_INVALID_HANDLE;

    if (response->mids == nullptr || response->NumFrameActual == 0)
        return MFX_ERR_NONE;

    mfxStatus sts = MFX_ERR_NONE;

    // check whether response is an external decoder response
    std::list<UniqueResponse>::iterator i =
        std::find_if( m_ExtResponses.begin(), m_ExtResponses.end(), std::bind1st(IsSame(), *response));

    if (i != m_ExtResponses.end())
    {
        if ((--i->m_refCount) == 0)
        {
            sts = ReleaseResponse(response);
            m_ExtResponses.erase(i);
        }
        return sts;
    }

    // if not found so far, then search in internal responses
    std::list<mfxFrameAllocResponse>::iterator i2 =
        std::find_if(m_responses.begin(), m_responses.end(), std::bind1st(IsSame(), *response));

    if (i2 != m_responses.end())
    {
        sts = ReleaseResponse(response);
        m_responses.erase(i2);
        return sts;
    }

    // not found anywhere, report an error
    return MFX_ERR_INVALID_HANDLE;
}

mfxStatus BaseFrameAllocator::Close()
{
    std::list<UniqueResponse> ::iterator i;
    for (i = m_ExtResponses.begin(); i!= m_ExtResponses.end(); i++)
    {
        ReleaseResponse(&*i);
    }
    m_ExtResponses.clear();

    std::list<mfxFrameAllocResponse> ::iterator i2;
    for (i2 = m_responses.begin(); i2!= m_responses.end(); i2++)
    {
        ReleaseResponse(&*i2);
    }

    return MFX_ERR_NONE;
}

#if defined(_WIN32) || defined(_WIN64)
#if defined(MFX_D3D11_ENABLED)

#define et_alloc_printf(...)
//#define et_alloc_printf printf

#include <objbase.h>
#include <initguid.h>
#include <iterator>

//for generating sequence of mfx handles
template <typename T>
struct sequence {
    T x;
    sequence(T seed) : x(seed) { }
};

template <>
struct sequence<mfxHDL> {
    mfxHDL x;
    sequence(mfxHDL seed) : x(seed) { }

    mfxHDL operator ()()
    {
        mfxHDL y = x;
        x = (mfxHDL)(1 + (size_t)(x));
        return y;
    }
};


D3D11FrameAllocator::D3D11FrameAllocator()
{
    m_pDeviceContext = NULL;
}

D3D11FrameAllocator::~D3D11FrameAllocator()
{
    Close();
}

D3D11FrameAllocator::TextureSubResource  D3D11FrameAllocator::GetResourceFromMid(mfxMemId mid)
{
    size_t index = (size_t)MFXReadWriteMid(mid).raw() - 1;

    if (m_memIdMap.size() <= index)
        return TextureSubResource();

    //reverse iterator dereferencing
    TextureResource * p = &(*m_memIdMap[index]);
    if (!p->bAlloc)
        return TextureSubResource();

    return TextureSubResource(p, mid);
}

mfxStatus D3D11FrameAllocator::Init(mfxAllocatorParams *pParams)
{
    D3D11AllocatorParams *pd3d11Params = 0;
    pd3d11Params = dynamic_cast<D3D11AllocatorParams *>(pParams);

    MFX_CHECK(pd3d11Params && pd3d11Params->pDevice, MFX_ERR_NOT_INITIALIZED);

    m_initParams = *pd3d11Params;
    m_pDeviceContext.Release();
    pd3d11Params->pDevice->GetImmediateContext(&m_pDeviceContext);

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::Close()
{
    mfxStatus sts = BaseFrameAllocator::Close();
    for (referenceType i = m_resourcesByRequest.begin(); i != m_resourcesByRequest.end(); i++)
    {
        i->Release();
    }
    m_resourcesByRequest.clear();
    m_memIdMap.clear();
    m_pDeviceContext.Release();
    return sts;
}

mfxStatus D3D11FrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    HRESULT hRes = S_OK;

    D3D11_TEXTURE2D_DESC desc = { 0 };
    D3D11_MAPPED_SUBRESOURCE lockedRect = { 0 };


    //check that texture exists
    TextureSubResource sr = GetResourceFromMid(mid);
    MFX_CHECK(sr.GetTexture(), MFX_ERR_LOCK_MEMORY);

    D3D11_MAP mapType = D3D11_MAP_READ;
    UINT mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;
    {
        if (NULL == sr.GetStaging())
        {
            hRes = m_pDeviceContext->Map(sr.GetTexture(), sr.GetSubResource(), D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &lockedRect);
            desc.Format = DXGI_FORMAT_P8;
        }
        else
        {
            sr.GetTexture()->GetDesc(&desc);

            if (DXGI_FORMAT_NV12 != desc.Format &&
                DXGI_FORMAT_420_OPAQUE != desc.Format &&
                DXGI_FORMAT_P010 != desc.Format &&
                DXGI_FORMAT_YUY2 != desc.Format &&
                DXGI_FORMAT_P8 != desc.Format &&
                DXGI_FORMAT_B8G8R8A8_UNORM != desc.Format &&
                DXGI_FORMAT_R8G8B8A8_UNORM != desc.Format &&
                DXGI_FORMAT_R10G10B10A2_UNORM != desc.Format &&
                DXGI_FORMAT_AYUV != desc.Format  &&
                DXGI_FORMAT_R16_UINT != desc.Format &&
                DXGI_FORMAT_R16_UNORM != desc.Format &&
                DXGI_FORMAT_R16_TYPELESS != desc.Format &&
                DXGI_FORMAT_Y210 != desc.Format &&
                DXGI_FORMAT_Y410 != desc.Format &&
                DXGI_FORMAT_P016 != desc.Format &&
                DXGI_FORMAT_Y216 != desc.Format &&
                DXGI_FORMAT_Y416 != desc.Format)
            {
                MFX_RETURN(MFX_ERR_LOCK_MEMORY);
            }

            //coping data only in case user wants to read from stored surface
            {

                if (MFXReadWriteMid(mid, MFXReadWriteMid::reuse).isRead())
                {
                    m_pDeviceContext->CopySubresourceRegion(sr.GetStaging(), 0, 0, 0, 0, sr.GetTexture(), sr.GetSubResource(), NULL);
                }

                do
                {
                    hRes = m_pDeviceContext->Map(sr.GetStaging(), 0, mapType, mapFlags, &lockedRect);
                    if (S_OK != hRes && DXGI_ERROR_WAS_STILL_DRAWING != hRes)
                    {
                        et_alloc_printf("ERROR: m_pDeviceContext->Map = 0x%X\n", hRes);
                    }
                } while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);
            }

        }
    }

    MFX_CHECK(!FAILED(hRes), MFX_ERR_LOCK_MEMORY);

    ptr->PitchHigh = (mfxU16)(lockedRect.RowPitch / (1 << 16));
    ptr->PitchLow = (mfxU16)(lockedRect.RowPitch % (1 << 16));

    switch (desc.Format)
    {
    case DXGI_FORMAT_NV12:
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->U = (mfxU8 *)lockedRect.pData + desc.Height * lockedRect.RowPitch;
        ptr->V = ptr->U + 1;
        break;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->U = (mfxU8 *)lockedRect.pData + desc.Height * lockedRect.RowPitch;
        ptr->V = ptr->U + 1;
        break;

    case DXGI_FORMAT_420_OPAQUE: // can be unsupported by standard ms guid
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->V = ptr->Y + desc.Height * lockedRect.RowPitch;
        ptr->U = ptr->V + (desc.Height * lockedRect.RowPitch) / 4;

        break;

    case DXGI_FORMAT_YUY2:
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;

        break;

    case DXGI_FORMAT_P8:
        ptr->Y = (mfxU8 *)lockedRect.pData;
        ptr->U = 0;
        ptr->V = 0;

        break;

    case DXGI_FORMAT_B8G8R8A8_UNORM:
        ptr->B = (mfxU8 *)lockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;

        break;

    case DXGI_FORMAT_R8G8B8A8_UNORM:
        ptr->R = (mfxU8 *)lockedRect.pData;
        ptr->G = ptr->R + 1;
        ptr->B = ptr->R + 2;
        ptr->A = ptr->R + 3;

        break;

    case DXGI_FORMAT_R10G10B10A2_UNORM:
        ptr->B = (mfxU8 *)lockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;

        break;

    case DXGI_FORMAT_AYUV:
        ptr->B = (mfxU8 *)lockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_TYPELESS:
        ptr->Y16 = (mfxU16 *)lockedRect.pData;
        ptr->U16 = 0;
        ptr->V16 = 0;

        break;

    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        ptr->Y16 = (mfxU16*)lockedRect.pData;
        ptr->U16 = ptr->Y16 + 1;
        ptr->V16 = ptr->U16 + 3;
        break;

    case DXGI_FORMAT_Y410:
        ptr->Y410 = (mfxY410 *)lockedRect.pData;
        ptr->Y = 0;
        ptr->V = 0;
        ptr->A = 0;
        break;

    case DXGI_FORMAT_Y416:
        ptr->U16 = (mfxU16*)lockedRect.pData;
        ptr->Y16 = ptr->U16 + 1;
        ptr->V16 = ptr->Y16 + 1;
        ptr->A = (mfxU8 *)(ptr->V16 + 1);
        break;

    default:

        MFX_RETURN(MFX_ERR_LOCK_MEMORY);
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    //check that texture exists
    TextureSubResource sr = GetResourceFromMid(mid);
    MFX_CHECK(sr.GetTexture(), MFX_ERR_LOCK_MEMORY);

    if (NULL == sr.GetStaging())
    {
        m_pDeviceContext->Unmap(sr.GetTexture(), sr.GetSubResource());
    }
    else
    {
        m_pDeviceContext->Unmap(sr.GetStaging(), 0);
        //only if user wrote something to texture
        if (MFXReadWriteMid(mid, MFXReadWriteMid::reuse).isWrite())
        {
            m_pDeviceContext->CopySubresourceRegion(sr.GetTexture(), sr.GetSubResource(), 0, 0, 0, sr.GetStaging(), 0, NULL);
        }
    }

    if (ptr)
    {
        ptr->PitchHigh = 0;
        ptr->PitchLow = 0;
        ptr->U = ptr->V = ptr->Y = 0;
        ptr->A = ptr->R = ptr->G = ptr->B = 0;
    }

    return MFX_ERR_NONE;
}


mfxStatus D3D11FrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    MFX_CHECK(handle, MFX_ERR_INVALID_HANDLE);

    TextureSubResource sr = GetResourceFromMid(mid);

    MFX_CHECK(sr.GetTexture(), MFX_ERR_INVALID_HANDLE);

    mfxHDLPair *pPair = (mfxHDLPair*)handle;

    pPair->first = sr.GetTexture();
    pPair->second = (mfxHDL)(UINT_PTR)sr.GetSubResource();

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    MFX_CHECK(((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) != 0), MFX_ERR_UNSUPPORTED);
    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    MFX_CHECK(response, MFX_ERR_NULL_PTR);

    if (response->mids && 0 != response->NumFrameActual)
    {
        //check whether texture exsist
        TextureSubResource sr = GetResourceFromMid(response->mids[0]);

        MFX_CHECK(sr.GetTexture(), MFX_ERR_NULL_PTR);

        sr.Release();

        //if texture is last it is possible to remove also all handles from map to reduce fragmentation
        //search for allocated chunk
        if (m_resourcesByRequest.end() == std::find_if(m_resourcesByRequest.begin(), m_resourcesByRequest.end(), TextureResource::isAllocated))
        {
            m_resourcesByRequest.clear();
            m_memIdMap.clear();
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11FrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    HRESULT hRes;

    DXGI_FORMAT colorFormat = ConverColortFormat(request->Info.FourCC);

    if (DXGI_FORMAT_UNKNOWN == colorFormat)
    {
        et_alloc_printf("Unknown format: %c%c%c%c\n", ((char *)&request->Info.FourCC)[0], ((char *)&request->Info.FourCC)[1], ((char *)&request->Info.FourCC)[2], ((char *)&request->Info.FourCC)[3]);
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    TextureResource newTexture;

    if (request->Info.FourCC == MFX_FOURCC_P8)
    {
        D3D11_BUFFER_DESC desc = { 0 };

        desc.ByteWidth = request->Info.Width * request->Info.Height;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        ID3D11Buffer * buffer = 0;
        hRes = m_initParams.pDevice->CreateBuffer(&desc, 0, &buffer);
        if (FAILED(hRes))
            MFX_RETURN(MFX_ERR_MEMORY_ALLOC);

        newTexture.textures.push_back(reinterpret_cast<ID3D11Texture2D *>(buffer));
    }
    else
    {
        D3D11_TEXTURE2D_DESC desc = { 0 };

        desc.Width = request->Info.Width;
        desc.Height = request->Info.Height;

        desc.MipLevels = 1;
        //number of subresources is 1 in case of not single texture
        desc.ArraySize = m_initParams.bUseSingleTexture ? request->NumFrameSuggested : 1;
        desc.Format = ConverColortFormat(request->Info.FourCC);
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.MiscFlags = m_initParams.uncompressedResourceMiscFlags;

        if ((request->Type&MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET) && (request->Type & MFX_MEMTYPE_INTERNAL_FRAME))
        {
            desc.BindFlags = D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER;
        }
        else
            desc.BindFlags = D3D11_BIND_DECODER;

        if ((DXGI_FORMAT_B8G8R8A8_UNORM == desc.Format) ||
            (DXGI_FORMAT_R8G8B8A8_UNORM == desc.Format) ||
            (DXGI_FORMAT_R10G10B10A2_UNORM == desc.Format))
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (desc.ArraySize > 2)
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
        }

        if ((MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
            (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type))
        {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (desc.ArraySize > 2)
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
        }

        if (request->Type&MFX_MEMTYPE_SHARED_RESOURCE)
        {
            desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        }

        if (DXGI_FORMAT_P8 == desc.Format)
        {
            desc.BindFlags = 0;
        }

        if (DXGI_FORMAT_R16_TYPELESS == colorFormat)
        {
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            RESOURCE_EXTENSION extnDesc;
            ZeroMemory(&extnDesc, sizeof(RESOURCE_EXTENSION));
            memcpy(&extnDesc.Key[0], RESOURCE_EXTENSION_KEY, sizeof(extnDesc.Key));
            extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
            extnDesc.Type = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
            // TODO: Pass real Bayer type
            extnDesc.Data[0] = RESOURCE_EXTENSION_CAMERA_PIPE::INPUT_FORMAT_IRW0;
            hRes = SetResourceExtension(&extnDesc);
            if (FAILED(hRes))
            {
                et_alloc_printf("SetResourceExtension failed, hr = 0x%X\n", hRes);
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
            }
        }

        ID3D11Texture2D* pTexture2D;

        if (m_initParams.bIsRawSurfLinear)
        {
            if (desc.Format == DXGI_FORMAT_NV12 || desc.Format == DXGI_FORMAT_P010)
            {
                desc.BindFlags = D3D11_BIND_DECODER;
            }
            else if (desc.Format == DXGI_FORMAT_Y410 ||
                desc.Format == DXGI_FORMAT_Y210 ||
                desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM ||
                desc.Format == DXGI_FORMAT_YUY2 ||
                desc.Format == DXGI_FORMAT_AYUV)
            {
                desc.BindFlags = 0;
            }
        }

        for (size_t i = 0; i < request->NumFrameSuggested / desc.ArraySize; i++)
        {
            hRes = m_initParams.pDevice->CreateTexture2D(&desc, NULL, &pTexture2D);

            if (FAILED(hRes))
            {
                et_alloc_printf("CreateTexture2D(%zd) failed, hr = 0x%X\n", i, hRes);
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
            }
            newTexture.textures.push_back(pTexture2D);
        }

        desc.ArraySize = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;

        for (size_t i = 0; i < request->NumFrameSuggested; i++)
        {
            hRes = m_initParams.pDevice->CreateTexture2D(&desc, NULL, &pTexture2D);

            if (FAILED(hRes))
            {
                et_alloc_printf("Create staging texture(%zd) failed hr = 0x%X\n", i, hRes);
                MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
            }
            newTexture.stagingTexture.push_back(pTexture2D);
        }
    }

    // mapping to self created handles array, starting from zero or from last assigned handle + 1
    sequence<mfxHDL> seq_initializer(m_resourcesByRequest.empty() ? 0 : m_resourcesByRequest.back().outerMids.back());

    //incrementing starting index
    //1. 0(NULL) is invalid memid
    //2. back is last index not new one
    seq_initializer();

    std::generate_n(std::back_inserter(newTexture.outerMids), request->NumFrameSuggested, seq_initializer);

    //saving texture resources
    m_resourcesByRequest.push_back(newTexture);

    //providing pointer to mids externally
    response->mids = &m_resourcesByRequest.back().outerMids.front();
    response->NumFrameActual = request->NumFrameSuggested;

    //iterator prior end()
    std::list <TextureResource>::iterator it_last = m_resourcesByRequest.end();
    //fill map
    std::fill_n(std::back_inserter(m_memIdMap), request->NumFrameSuggested, --it_last);

    return MFX_ERR_NONE;
}

DXGI_FORMAT D3D11FrameAllocator::ConverColortFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return DXGI_FORMAT_NV12;

    case MFX_FOURCC_P010:
        return DXGI_FORMAT_P010;

    case MFX_FOURCC_A2RGB10:
        return DXGI_FORMAT_R10G10B10A2_UNORM;

    case MFX_FOURCC_YUY2:
        return DXGI_FORMAT_YUY2;

    case MFX_FOURCC_RGB4:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case MFX_FOURCC_BGR4:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case MFX_FOURCC_AYUV:
    case DXGI_FORMAT_AYUV:
        return DXGI_FORMAT_AYUV;

    case MFX_FOURCC_P8:
    case MFX_FOURCC_P8_TEXTURE:
        return DXGI_FORMAT_P8;

    case MFX_FOURCC_R16:
        return DXGI_FORMAT_R16_TYPELESS;

    case MFX_FOURCC_Y210:
        return DXGI_FORMAT_Y210;

    case MFX_FOURCC_Y410:
        return DXGI_FORMAT_Y410;

    case MFX_FOURCC_P016:
        return DXGI_FORMAT_P016;

    case MFX_FOURCC_Y216:
        return DXGI_FORMAT_Y216;

    case MFX_FOURCC_Y416:
        return DXGI_FORMAT_Y416;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

#endif // MFX_D3D11_ENABLED


#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N','V','1','2')
#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y','V','1','2')
#define D3DFMT_P010 (D3DFORMAT)MAKEFOURCC('P','0','1','0')
#define D3DFMT_Y210 (D3DFORMAT)MFX_FOURCC_Y210
#define D3DFMT_P210 (D3DFORMAT)MFX_FOURCC_P210
#define D3DFMT_Y410 (D3DFORMAT)MFX_FOURCC_Y410
#define D3DFMT_AYUV (D3DFORMAT)MFX_FOURCC_AYUV
#define D3DFMT_P016 (D3DFORMAT)MFX_FOURCC_P016
#define D3DFMT_Y216 (D3DFORMAT)MFX_FOURCC_Y216
#define D3DFMT_Y416 (D3DFORMAT)MFX_FOURCC_Y416

D3DFORMAT ConvertMfxFourccToD3dFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return D3DFMT_NV12;
    case MFX_FOURCC_YV12:
        return D3DFMT_YV12;
    case MFX_FOURCC_YUY2:
        return D3DFMT_YUY2;
    case MFX_FOURCC_RGB3:
        return D3DFMT_R8G8B8;
    case MFX_FOURCC_RGB4:
        return D3DFMT_A8R8G8B8;
    case MFX_FOURCC_BGR4:
        return D3DFMT_A8B8G8R8;
    case MFX_FOURCC_P8:
        return D3DFMT_P8;
    case MFX_FOURCC_P010:
        return D3DFMT_P010;
    case MFX_FOURCC_A2RGB10:
        return D3DFMT_A2R10G10B10;
    case MFX_FOURCC_R16:
        return D3DFMT_R16F;
    case MFX_FOURCC_ARGB16:
        return D3DFMT_A16B16G16R16;
    case MFX_FOURCC_P210:
        return D3DFMT_P210;
    case MFX_FOURCC_AYUV:
        return D3DFMT_AYUV;

    case MFX_FOURCC_Y210:
        return D3DFMT_Y210;
    case MFX_FOURCC_Y410:
        return D3DFMT_Y410;

    case MFX_FOURCC_P016:
        return D3DFMT_P016;
    case MFX_FOURCC_Y216:
        return D3DFMT_Y216;
    case MFX_FOURCC_Y416:
        return D3DFMT_Y416;

    default:
        return D3DFMT_UNKNOWN;
    }
}

class DeviceHandle
{
public:
    DeviceHandle(IDirect3DDeviceManager9* manager)
        : m_manager(manager)
        , m_handle(0)
    {
        if (manager)
        {
            HRESULT hr = manager->OpenDeviceHandle(&m_handle);
            if (FAILED(hr))
                m_manager = 0;
        }
    }

    ~DeviceHandle()
    {
        if (m_manager && m_handle)
            m_manager->CloseDeviceHandle(m_handle);
    }

    HANDLE Detach()
    {
        HANDLE tmp = m_handle;
        m_manager = 0;
        m_handle = 0;
        return tmp;
    }

    operator HANDLE()
    {
        return m_handle;
    }

    bool operator !() const
    {
        return m_handle == 0;
    }

protected:
    CComPtr<IDirect3DDeviceManager9> m_manager;
    HANDLE m_handle;
};

D3DFrameAllocator::D3DFrameAllocator()
    : m_decoderService(0), m_processorService(0), m_hDecoder(0), m_hProcessor(0), m_manager(0), m_surfaceUsage(0)
{
}

D3DFrameAllocator::~D3DFrameAllocator()
{
    Close();
}

mfxStatus D3DFrameAllocator::Init(mfxAllocatorParams *pParams)
{
    D3DAllocatorParams *pd3dParams = 0;
    pd3dParams = dynamic_cast<D3DAllocatorParams *>(pParams);
    if (!pd3dParams)
        return MFX_ERR_NOT_INITIALIZED;

    m_manager = pd3dParams->pManager;
    m_surfaceUsage = pd3dParams->surfaceUsage;

    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::Close()
{
    if (m_manager && m_hDecoder)
    {
        m_manager->CloseDeviceHandle(m_hDecoder);
        m_manager = 0;
        m_hDecoder = 0;
    }

    if (m_manager && m_hProcessor)
    {
        m_manager->CloseDeviceHandle(m_hProcessor);
        m_manager = 0;
        m_hProcessor = 0;
    }

    return BaseFrameAllocator::Close();
}

mfxStatus D3DFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *)mid;
    if (pSurface == 0)
        return MFX_ERR_INVALID_HANDLE;

    if (ptr == 0)
        return MFX_ERR_LOCK_MEMORY;

    D3DSURFACE_DESC desc;
    HRESULT hr = pSurface->GetDesc(&desc);
    if (FAILED(hr))
        return MFX_ERR_LOCK_MEMORY;

    if (desc.Format != D3DFMT_NV12 &&
        desc.Format != D3DFMT_YV12 &&
        desc.Format != D3DFMT_YUY2 &&
        desc.Format != D3DFMT_R8G8B8 &&
        desc.Format != D3DFMT_A8R8G8B8 &&
        desc.Format != D3DFMT_A8B8G8R8 &&
        desc.Format != D3DFMT_P8 &&
        desc.Format != D3DFMT_P010 &&
        desc.Format != D3DFMT_A2R10G10B10 &&
        desc.Format != D3DFMT_R16F &&
        desc.Format != D3DFMT_A16B16G16R16 &&
        desc.Format != D3DFMT_AYUV &&
        desc.Format != D3DFMT_Y210 &&
        desc.Format != D3DFMT_Y410 &&
        desc.Format != D3DFMT_P016 &&
        desc.Format != D3DFMT_Y216 &&
        desc.Format != D3DFMT_Y416)
        return MFX_ERR_LOCK_MEMORY;

    D3DLOCKED_RECT locked;

    hr = pSurface->LockRect(&locked, 0, D3DLOCK_NOSYSLOCK);
    if (FAILED(hr))
        return MFX_ERR_LOCK_MEMORY;

    ptr->PitchHigh = (mfxU16)(locked.Pitch / (1 << 16));
    ptr->PitchLow = (mfxU16)(locked.Pitch % (1 << 16));
    switch ((DWORD)desc.Format)
    {
    case D3DFMT_NV12:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = (mfxU8 *)locked.pBits + desc.Height * locked.Pitch;
        ptr->V = ptr->U + 1;
        break;

    case D3DFMT_P010:
    case D3DFMT_P016:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = (mfxU8 *)locked.pBits + desc.Height * locked.Pitch;
        ptr->V = ptr->U + 1;
        break;

    case D3DFMT_A2R10G10B10:
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;

    case D3DFMT_A16B16G16R16:
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->Y16 = (mfxU16 *)locked.pBits;
        ptr->G = ptr->B + 2;
        ptr->R = ptr->B + 4;
        ptr->A = ptr->B + 6;
        break;

    case D3DFMT_YV12:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->V = ptr->Y + desc.Height * locked.Pitch;
        ptr->U = ptr->V + (desc.Height * locked.Pitch) / 4;
        break;

    case D3DFMT_YUY2:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        break;

    case D3DFMT_R8G8B8:
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        break;

    case D3DFMT_A8R8G8B8:
    case D3DFMT_AYUV:
        ptr->B = (mfxU8 *)locked.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;

    case D3DFMT_A8B8G8R8:
        ptr->R = (mfxU8 *)locked.pBits;
        ptr->G = ptr->R + 1;
        ptr->B = ptr->R + 2;
        ptr->A = ptr->R + 3;
        break;

    case D3DFMT_P8:
        ptr->Y = (mfxU8 *)locked.pBits;
        ptr->U = 0;
        ptr->V = 0;
        break;

    case D3DFMT_R16F:
        ptr->Y16 = (mfxU16 *)locked.pBits;
        ptr->U16 = 0;
        ptr->V16 = 0;
        break;

    case D3DFMT_Y210:
    case D3DFMT_Y216:
        ptr->Y16 = (mfxU16*)locked.pBits;
        ptr->U16 = ptr->Y16 + 1;
        ptr->V16 = ptr->U16 + 3;
        break;

    case D3DFMT_Y410:
        ptr->Y410 = (mfxY410 *)locked.pBits;
        ptr->Y = 0;
        ptr->V = 0;
        ptr->A = 0;
        break;

    case D3DFMT_Y416:
        ptr->U16 = (mfxU16*)locked.pBits;
        ptr->Y16 = ptr->U16 + 1;
        ptr->V16 = ptr->Y16 + 1;
        ptr->A = (mfxU8 *)(ptr->V16 + 1);
        break;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    IDirect3DSurface9 *pSurface = (IDirect3DSurface9 *)mid;
    if (pSurface == 0)
        return MFX_ERR_INVALID_HANDLE;

    pSurface->UnlockRect();

    if (NULL != ptr)
    {
        ptr->PitchHigh = 0;
        ptr->PitchLow = 0;
        ptr->Y = 0;
        ptr->U = 0;
        ptr->V = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    if (handle == 0)
        return MFX_ERR_INVALID_HANDLE;

    *handle = mid;
    return MFX_ERR_NONE;
}

mfxStatus D3DFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    if ((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3DFrameAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    if (!response)
        return MFX_ERR_NULL_PTR;

    mfxStatus sts = MFX_ERR_NONE;

    if (response->mids)
    {
        for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        {
            if (response->mids[i])
            {
                IDirect3DSurface9* handle = 0;
                sts = GetFrameHDL(response->mids[i], (mfxHDL *)&handle);
                if (MFX_ERR_NONE != sts)
                    return sts;
                handle->Release();
            }
        }
    }

    delete[] response->mids;
    response->mids = 0;

    return sts;
}

mfxStatus D3DFrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    HRESULT hr;

    D3DFORMAT format = ConvertMfxFourccToD3dFormat(request->Info.FourCC);

    if (format == D3DFMT_UNKNOWN)
        return MFX_ERR_UNSUPPORTED;

    safe_array<mfxMemId> mids(new mfxMemId[request->NumFrameSuggested]);
    if (!mids.get())
        return MFX_ERR_MEMORY_ALLOC;

    DWORD   target;

    if (MFX_MEMTYPE_DXVA2_DECODER_TARGET & request->Type)
    {
        target = DXVA2_VideoDecoderRenderTarget;
    }
    else if (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET & request->Type)
    {
        target = DXVA2_VideoProcessorRenderTarget;
    }
    else
        return MFX_ERR_UNSUPPORTED;

    // VPP may require at input/output surfaces with color format other than NV12 (in case of color conversion)
    // VideoProcessorService must used to create such surfaces
    if (target == DXVA2_VideoProcessorRenderTarget)
    {
        if (!m_hProcessor)
        {
            DeviceHandle device = DeviceHandle(m_manager);

            if (!device)
                return MFX_ERR_MEMORY_ALLOC;

            CComPtr<IDirectXVideoProcessorService> service = 0;

            hr = m_manager->GetVideoService(device, IID_IDirectXVideoProcessorService, (void**)&service);

            if (FAILED(hr))
                return MFX_ERR_MEMORY_ALLOC;

            m_processorService = service;
            m_hProcessor = device.Detach();
        }

        hr = m_processorService->CreateSurface(
            request->Info.Width,
            request->Info.Height,
            request->NumFrameSuggested - 1,
            format,
            D3DPOOL_DEFAULT,
            m_surfaceUsage,
            target,
            (IDirect3DSurface9 **)mids.get(),
            NULL);
    }
    else
    {
        if (!m_hDecoder)
        {
            DeviceHandle device = DeviceHandle(m_manager);

            if (!device)
                return MFX_ERR_MEMORY_ALLOC;

            CComPtr<IDirectXVideoDecoderService> service = 0;

            hr = m_manager->GetVideoService(device, IID_IDirectXVideoDecoderService, (void**)&service);

            if (FAILED(hr))
                return MFX_ERR_MEMORY_ALLOC;

            m_decoderService = service;
            m_hDecoder = device.Detach();
        }

        hr = m_decoderService->CreateSurface(
            request->Info.Width,
            request->Info.Height,
            request->NumFrameSuggested - 1,
            format,
            D3DPOOL_DEFAULT,
            m_surfaceUsage,
            target,
            (IDirect3DSurface9 **)mids.get(),
            NULL);
    }

    if (FAILED(hr))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    response->mids = mids.release();
    response->NumFrameActual = request->NumFrameSuggested;
    return MFX_ERR_NONE;
}

#endif // #if defined(_WIN32) || defined(_WIN64)

#if defined(MFX_VA_LINUX)

#include <dlfcn.h>
#include <iostream>

namespace MfxLoader
{

    SimpleLoader::SimpleLoader(const char* name)
    {
        dlerror();
        so_handle = dlopen(name, RTLD_GLOBAL | RTLD_NOW);
        if (NULL == so_handle)
        {
            std::cerr << dlerror() << std::endl;
            throw std::runtime_error("Can't load library");
        }
    }

    void* SimpleLoader::GetFunction(const char* name)
    {
        void* fn_ptr = dlsym(so_handle, name);
        if (!fn_ptr)
            throw std::runtime_error("Can't find function");
        return fn_ptr;
    }

    SimpleLoader::~SimpleLoader()
    {
        if (so_handle)
            dlclose(so_handle);
    }

#define SIMPLE_LOADER_STRINGIFY1( x) #x
#define SIMPLE_LOADER_STRINGIFY(x) SIMPLE_LOADER_STRINGIFY1(x)
#define SIMPLE_LOADER_DECORATOR1(fun,suffix) fun ## _ ## suffix
#define SIMPLE_LOADER_DECORATOR(fun,suffix) SIMPLE_LOADER_DECORATOR1(fun,suffix)


    // Following macro applied on vaInitialize will give:  vaInitialize((vaInitialize_type)lib.GetFunction("vaInitialize"))
#define SIMPLE_LOADER_FUNCTION(name) name( (SIMPLE_LOADER_DECORATOR(name, type)) lib.GetFunction(SIMPLE_LOADER_STRINGIFY(name)) )


    VA_Proxy::VA_Proxy()
        : lib("libva.so.2")
        , SIMPLE_LOADER_FUNCTION(vaInitialize)
        , SIMPLE_LOADER_FUNCTION(vaTerminate)
        , SIMPLE_LOADER_FUNCTION(vaCreateSurfaces)
        , SIMPLE_LOADER_FUNCTION(vaDestroySurfaces)
        , SIMPLE_LOADER_FUNCTION(vaCreateBuffer)
        , SIMPLE_LOADER_FUNCTION(vaDestroyBuffer)
        , SIMPLE_LOADER_FUNCTION(vaMapBuffer)
        , SIMPLE_LOADER_FUNCTION(vaUnmapBuffer)
        , SIMPLE_LOADER_FUNCTION(vaDeriveImage)
        , SIMPLE_LOADER_FUNCTION(vaDestroyImage)
        , SIMPLE_LOADER_FUNCTION(vaSyncSurface)
    {
    }

    VA_Proxy::~VA_Proxy()
    {}


#undef SIMPLE_LOADER_FUNCTION

} // MfxLoader


mfxStatus va_to_mfx_status(VAStatus va_res)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    switch (va_res)
    {
    case VA_STATUS_SUCCESS:
        mfxRes = MFX_ERR_NONE;
        break;
    case VA_STATUS_ERROR_ALLOCATION_FAILED:
        mfxRes = MFX_ERR_MEMORY_ALLOC;
        break;
    case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
    case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
    case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
    case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
    case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
    case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
    case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
        mfxRes = MFX_ERR_UNSUPPORTED;
        break;
    case VA_STATUS_ERROR_INVALID_DISPLAY:
    case VA_STATUS_ERROR_INVALID_CONFIG:
    case VA_STATUS_ERROR_INVALID_CONTEXT:
    case VA_STATUS_ERROR_INVALID_SURFACE:
    case VA_STATUS_ERROR_INVALID_BUFFER:
    case VA_STATUS_ERROR_INVALID_IMAGE:
    case VA_STATUS_ERROR_INVALID_SUBPICTURE:
        mfxRes = MFX_ERR_NOT_INITIALIZED;
        break;
    case VA_STATUS_ERROR_INVALID_PARAMETER:
        mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
    default:
        mfxRes = MFX_ERR_UNKNOWN;
        break;
    }
    return mfxRes;
}


enum {
    MFX_FOURCC_VP8_NV12 = MFX_MAKEFOURCC('V', 'P', '8', 'N'),
    MFX_FOURCC_VP8_MBDATA = MFX_MAKEFOURCC('V', 'P', '8', 'M'),
    MFX_FOURCC_VP8_SEGMAP = MFX_MAKEFOURCC('V', 'P', '8', 'S'),
};

unsigned int ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return VA_FOURCC_NV12;
    case MFX_FOURCC_YUY2:
        return VA_FOURCC_YUY2;
    case MFX_FOURCC_YV12:
        return VA_FOURCC_YV12;
    case MFX_FOURCC_RGB4:
        return VA_FOURCC_ARGB;
    case MFX_FOURCC_A2RGB10:
        return VA_FOURCC_ARGB;  // rt format will be VA_RT_FORMAT_RGB32_10BPP
    case MFX_FOURCC_BGR4:
        return VA_FOURCC_ABGR;
    case MFX_FOURCC_P8:
        return VA_FOURCC_P208;
    case MFX_FOURCC_P010:
        return VA_FOURCC_P010;
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
        return VA_FOURCC_Y210;
    case MFX_FOURCC_Y410:
        return VA_FOURCC_Y410;
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_P016:
        return VA_FOURCC_P016;
    case MFX_FOURCC_Y216:
        return VA_FOURCC_Y216;
    case MFX_FOURCC_Y416:
        return VA_FOURCC_Y416;
#endif
    case MFX_FOURCC_AYUV:
        return VA_FOURCC_AYUV;
#if (MFX_VERSION >= 1028)
    case MFX_FOURCC_RGB565:
        return VA_FOURCC_RGB565;
    case MFX_FOURCC_RGBP:
        return VA_RT_FORMAT_RGBP;
#endif
    default:
        assert(!"unsupported fourcc");
        return 0;
    }
}

unsigned int ConvertVP8FourccToMfxFourcc(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_VP8_NV12:
    case MFX_FOURCC_VP8_MBDATA:
        return MFX_FOURCC_NV12;
    case MFX_FOURCC_VP8_SEGMAP:
        return MFX_FOURCC_P8;

    default:
        return fourcc;
    }
}

vaapiFrameAllocator::vaapiFrameAllocator()
    : m_dpy(0),
    m_libva(new MfxLoader::VA_Proxy),
    m_bAdaptivePlayback(false),
    m_Width(0),
    m_Height(0)
{
}

vaapiFrameAllocator::~vaapiFrameAllocator()
{
    Close();
    delete m_libva;
}

mfxStatus vaapiFrameAllocator::Init(mfxAllocatorParams* pParams)
{
    vaapiAllocatorParams* p_vaapiParams = dynamic_cast<vaapiAllocatorParams*>(pParams);

    if ((NULL == p_vaapiParams) || (NULL == p_vaapiParams->m_dpy))
        return MFX_ERR_NOT_INITIALIZED;

    m_dpy = p_vaapiParams->m_dpy;
    m_bAdaptivePlayback = p_vaapiParams->bAdaptivePlayback;
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::CheckRequestType(mfxFrameAllocRequest* request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    if ((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus vaapiFrameAllocator::Close()
{
    return BaseFrameAllocator::Close();
}

mfxStatus vaapiFrameAllocator::AllocImpl(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_res = VA_STATUS_SUCCESS;
    unsigned int va_fourcc = 0;
    VASurfaceID* surfaces = NULL;
    vaapiMemId* vaapi_mids = NULL, * vaapi_mid = NULL;
    mfxMemId* mids = NULL;
    mfxU32 fourcc = request->Info.FourCC;
    mfxU16 surfaces_num = request->NumFrameSuggested, numAllocated = 0, i = 0;
    bool bCreateSrfSucceeded = false;

    memset(response, 0, sizeof(mfxFrameAllocResponse));

    // VP8 hybrid driver has weird requirements for allocation of surfaces/buffers for VP8 encoding
    // to comply with them additional logic is required to support regular and VP8 hybrid allocation pathes
    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(fourcc);
    va_fourcc = ConvertMfxFourccToVAFormat(mfx_fourcc);
    if (!va_fourcc || ((VA_FOURCC_NV12 != va_fourcc) &&
        (VA_FOURCC_YV12 != va_fourcc) &&
        (VA_FOURCC_YUY2 != va_fourcc) &&
        (VA_FOURCC_ARGB != va_fourcc) &&
        (VA_FOURCC_ABGR != va_fourcc) &&
        (VA_FOURCC_P208 != va_fourcc) &&
        (VA_FOURCC_P010 != va_fourcc) &&
        (VA_FOURCC_YUY2 != va_fourcc) &&
#if (MFX_VERSION >= 1027)
        (VA_FOURCC_Y210 != va_fourcc) &&
        (VA_FOURCC_Y410 != va_fourcc) &&
#endif
#if (MFX_VERSION >= 1028)
        (VA_FOURCC_RGB565 != va_fourcc) &&
        (VA_RT_FORMAT_RGBP != va_fourcc) &&
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        (VA_FOURCC_P016 != va_fourcc) &&
        (VA_FOURCC_Y216 != va_fourcc) &&
        (VA_FOURCC_Y416 != va_fourcc) &&
#endif
        (VA_FOURCC_AYUV != va_fourcc)))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    if (!surfaces_num)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        surfaces = (VASurfaceID*)calloc(surfaces_num, sizeof(VASurfaceID));
        vaapi_mids = (vaapiMemId*)calloc(surfaces_num, sizeof(vaapiMemId));
        mids = (mfxMemId*)calloc(surfaces_num, sizeof(mfxMemId));
        if ((NULL == surfaces) || (NULL == vaapi_mids) || (NULL == mids)) mfx_res = MFX_ERR_MEMORY_ALLOC;
    }

    m_Width = request->Info.Width;
    m_Height = request->Info.Height;

    if (MFX_ERR_NONE == mfx_res)
    {
        if (m_bAdaptivePlayback)
        {
            for (i = 0; i < surfaces_num; ++i)
            {
                vaapi_mid = &(vaapi_mids[i]);
                vaapi_mid->m_fourcc = fourcc;
                surfaces[i] = (VASurfaceID)VA_INVALID_ID;
                vaapi_mid->m_surface = &surfaces[i];
                mids[i] = vaapi_mid;
            }
            response->mids = mids;
            response->NumFrameActual = surfaces_num;
            return MFX_ERR_NONE;
        }

        if (VA_FOURCC_P208 != va_fourcc)
        {
            unsigned int format;
            VASurfaceAttrib attrib[2];
            VASurfaceAttrib* pAttrib = &attrib[0];
            int attrCnt = 0;

            attrib[attrCnt].type = VASurfaceAttribPixelFormat;
            attrib[attrCnt].flags = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[attrCnt].value.type = VAGenericValueTypeInteger;
            attrib[attrCnt++].value.value.i = (va_fourcc == VA_RT_FORMAT_RGBP ? VA_FOURCC_RGBP : va_fourcc);
            format = va_fourcc;

            if ((fourcc == MFX_FOURCC_VP8_NV12) ||
                ((MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET & request->Type)
                    && ((fourcc == MFX_FOURCC_RGB4) || (fourcc == MFX_FOURCC_BGR4))))
            {
                /*
                 *  special configuration for NV12 surf allocation for VP8 hybrid encoder and
                 *  RGB32 for JPEG is required
                 */
                attrib[attrCnt].type = (VASurfaceAttribType)VASurfaceAttribUsageHint;
                attrib[attrCnt].flags = VA_SURFACE_ATTRIB_SETTABLE;
                attrib[attrCnt].value.type = VAGenericValueTypeInteger;
                attrib[attrCnt++].value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
            }
            else if (fourcc == MFX_FOURCC_VP8_MBDATA)
            {
                // special configuration for MB data surf allocation for VP8 hybrid encoder is required
                attrib[0].value.value.i = VA_FOURCC_P208;
                format = VA_FOURCC_P208;
            }
            else if (va_fourcc == VA_FOURCC_NV12)
            {
                format = VA_RT_FORMAT_YUV420;
            }
            else if (fourcc == MFX_FOURCC_A2RGB10)
            {
                format = VA_RT_FORMAT_RGB32_10BPP;
            }
#if VA_CHECK_VERSION(1,2,0)
            else if (va_fourcc == VA_FOURCC_P010)
            {
                format = VA_RT_FORMAT_YUV420_10;
            }
#endif

            va_res = m_libva->vaCreateSurfaces(m_dpy,
                format,
                request->Info.Width, request->Info.Height,
                surfaces,
                surfaces_num,
                pAttrib, pAttrib ? attrCnt : 0);
            mfx_res = va_to_mfx_status(va_res);
            bCreateSrfSucceeded = (MFX_ERR_NONE == mfx_res);
        }
        else
        {
            VAContextID context_id = request->AllocId;
            int codedbuf_size, codedbuf_num;

            VABufferType codedbuf_type;
            if (fourcc == MFX_FOURCC_VP8_SEGMAP)
            {
                codedbuf_size = request->Info.Width;
                codedbuf_num = request->Info.Height;
                codedbuf_type = VAEncMacroblockMapBufferType;
            }
            else
            {
                int width32 = 32 * ((request->Info.Width + 31) >> 5);
                int height32 = 32 * ((request->Info.Height + 31) >> 5);
                codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16));
                codedbuf_num = 1;
                codedbuf_type = VAEncCodedBufferType;
            }

            for (numAllocated = 0; numAllocated < surfaces_num; numAllocated++)
            {
                VABufferID coded_buf;

                va_res = m_libva->vaCreateBuffer(m_dpy,
                    context_id,
                    codedbuf_type,
                    codedbuf_size,
                    codedbuf_num,
                    NULL,
                    &coded_buf);
                mfx_res = va_to_mfx_status(va_res);
                if (MFX_ERR_NONE != mfx_res) break;
                surfaces[numAllocated] = coded_buf;
            }
        }

    }
    if (MFX_ERR_NONE == mfx_res)
    {
        for (i = 0; i < surfaces_num; ++i)
        {
            vaapi_mid = &(vaapi_mids[i]);
            vaapi_mid->m_fourcc = fourcc;
            vaapi_mid->m_surface = &(surfaces[i]);
            mids[i] = vaapi_mid;
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        response->mids = mids;
        response->NumFrameActual = surfaces_num;
    }
    else // i.e. MFX_ERR_NONE != mfx_res
    {
        response->mids = NULL;
        response->NumFrameActual = 0;
        if (VA_FOURCC_P208 != va_fourcc
            || fourcc == MFX_FOURCC_VP8_MBDATA)
        {
            if (bCreateSrfSucceeded)
                m_libva->vaDestroySurfaces(m_dpy, surfaces, surfaces_num);
        }
        else
        {
            for (i = 0; i < numAllocated; i++)
                m_libva->vaDestroyBuffer(m_dpy, surfaces[i]);
        }
        if (mids)
        {
            free(mids);
            mids = NULL;
        }
        if (vaapi_mids) { free(vaapi_mids); vaapi_mids = NULL; }
        if (surfaces) { free(surfaces); surfaces = NULL; }
    }
    return mfx_res;
}

mfxStatus vaapiFrameAllocator::ReleaseResponse(mfxFrameAllocResponse* response)
{
    vaapiMemId* vaapi_mids = NULL;
    VASurfaceID* surfaces = NULL;
    mfxU32 i = 0;
    bool isBitstreamMemory = false;

    if (!response) return MFX_ERR_NULL_PTR;

    if (response->mids)
    {
        vaapi_mids = (vaapiMemId*)(response->mids[0]);
        mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mids->m_fourcc);
        isBitstreamMemory = (MFX_FOURCC_P8 == mfx_fourcc) ? true : false;
        surfaces = vaapi_mids->m_surface;
        for (i = 0; i < response->NumFrameActual; ++i)
        {
            if (MFX_FOURCC_P8 == vaapi_mids[i].m_fourcc) m_libva->vaDestroyBuffer(m_dpy, surfaces[i]);
            else if (vaapi_mids[i].m_sys_buffer) free(vaapi_mids[i].m_sys_buffer);
        }
        free(vaapi_mids);
        free(response->mids);
        response->mids = NULL;

        if (!isBitstreamMemory) m_libva->vaDestroySurfaces(m_dpy, surfaces, response->NumFrameActual);
        free(surfaces);
    }
    response->NumFrameActual = 0;
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData* ptr)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_res = VA_STATUS_SUCCESS;
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;

    if (!vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    mfxU8* pBuffer = 0;
    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mid->m_fourcc);

    if ((VASurfaceID)VA_INVALID_ID == *(vaapi_mid->m_surface))
    {
        if (VA_FOURCC_P208 != vaapi_mid->m_fourcc)
        {
            unsigned int format;
            VASurfaceAttrib attrib[2];
            VASurfaceAttrib* pAttrib = &attrib[0];
            int attrCnt = 0;

            attrib[attrCnt].type = VASurfaceAttribPixelFormat;
            attrib[attrCnt].flags = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[attrCnt].value.type = VAGenericValueTypeInteger;
            attrib[attrCnt++].value.value.i = (vaapi_mid->m_fourcc == VA_RT_FORMAT_RGBP ? VA_FOURCC_RGBP : vaapi_mid->m_fourcc);
            format = vaapi_mid->m_fourcc;

            if (mfx_fourcc == MFX_FOURCC_VP8_NV12)
            {
                // special configuration for NV12 surf allocation for VP8 hybrid encoder is required
                attrib[attrCnt].type = (VASurfaceAttribType)VASurfaceAttribUsageHint;
                attrib[attrCnt].flags = VA_SURFACE_ATTRIB_SETTABLE;
                attrib[attrCnt].value.type = VAGenericValueTypeInteger;
                attrib[attrCnt++].value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
            }
            else if (mfx_fourcc == MFX_FOURCC_VP8_MBDATA)
            {
                // special configuration for MB data surf allocation for VP8 hybrid encoder is required
                attrib[0].value.value.i = VA_FOURCC_P208;
                format = VA_FOURCC_P208;
            }
            else if (vaapi_mid->m_fourcc == VA_FOURCC_NV12)
            {
                format = VA_RT_FORMAT_YUV420;
            }

            va_res = m_libva->vaCreateSurfaces(m_dpy,
                format,
                m_Width, m_Height,
                vaapi_mid->m_surface,
                1,
                pAttrib, pAttrib ? attrCnt : 0);
            mfx_res = va_to_mfx_status(va_res);
        }
        else
        {
            int codedbuf_size, codedbuf_num;

            VABufferType codedbuf_type;
            if (mfx_fourcc == MFX_FOURCC_VP8_SEGMAP)
            {
                codedbuf_size = m_Width;
                codedbuf_num = m_Height;
                codedbuf_type = VAEncMacroblockMapBufferType;
            }
            else
            {
                int width32 = 32 * ((m_Width + 31) >> 5);
                int height32 = 32 * ((m_Height + 31) >> 5);
                codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16));
                codedbuf_num = 1;
                codedbuf_type = VAEncCodedBufferType;
            }
            VABufferID coded_buf;

            va_res = m_libva->vaCreateBuffer(m_dpy,
                VA_INVALID_ID,
                codedbuf_type,
                codedbuf_size,
                codedbuf_num,
                NULL,
                &coded_buf);
            mfx_res = va_to_mfx_status(va_res);
            //vaapi_mid->m_surface = coded_buf;
        }
        return MFX_ERR_NONE;
    }

    if (MFX_FOURCC_P8 == mfx_fourcc)   // bitstream processing
    {
        VACodedBufferSegment* coded_buffer_segment;
        if (vaapi_mid->m_fourcc == MFX_FOURCC_VP8_SEGMAP)
            va_res = m_libva->vaMapBuffer(m_dpy, *(vaapi_mid->m_surface), (void**)(&pBuffer));
        else
            va_res = m_libva->vaMapBuffer(m_dpy, *(vaapi_mid->m_surface), (void**)(&coded_buffer_segment));
        mfx_res = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE == mfx_res)
        {
            if (vaapi_mid->m_fourcc == MFX_FOURCC_VP8_SEGMAP)
                ptr->Y = pBuffer;
            else
                ptr->Y = (mfxU8*)coded_buffer_segment->buf;

        }
    }
    else   // Image processing
    {
        va_res = m_libva->vaDeriveImage(m_dpy, *(vaapi_mid->m_surface), &(vaapi_mid->m_image));
        mfx_res = va_to_mfx_status(va_res);

        if (MFX_ERR_NONE == mfx_res)
        {
            va_res = m_libva->vaMapBuffer(m_dpy, vaapi_mid->m_image.buf, (void**)&pBuffer);
            mfx_res = va_to_mfx_status(va_res);
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            switch (vaapi_mid->m_image.format.fourcc)
            {
            case VA_FOURCC_NV12:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->V = ptr->U + 1;
                }
                break;
            case VA_FOURCC_YV12:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->V = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[2];
                }
                break;
            case VA_FOURCC_YUY2:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = ptr->Y + 1;
                    ptr->V = ptr->Y + 3;
                }
                break;
            case VA_FOURCC_ARGB:
                if (mfx_fourcc == MFX_FOURCC_RGB4)
                {
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B + 1;
                    ptr->R = ptr->B + 2;
                    ptr->A = ptr->B + 3;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
#ifndef ANDROID
            case VA_FOURCC_A2R10G10B10:
                if (mfx_fourcc == MFX_FOURCC_A2RGB10)
                {
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B;
                    ptr->R = ptr->B;
                    ptr->A = ptr->B;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
#endif
            case VA_FOURCC_ABGR:
                if (mfx_fourcc == MFX_FOURCC_BGR4)
                {
                    ptr->R = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->R + 1;
                    ptr->B = ptr->R + 2;
                    ptr->A = ptr->R + 3;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
#if (MFX_VERSION >= 1028)
            case VA_FOURCC_RGB565:
                if (mfx_fourcc == MFX_FOURCC_RGB565)
                {
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B;
                    ptr->R = ptr->B;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
            case MFX_FOURCC_RGBP:
                if (mfx_fourcc == MFX_FOURCC_RGBP)
                {
                    ptr->R = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[2];
                }
#endif
            case VA_FOURCC_P208:
                if (mfx_fourcc == MFX_FOURCC_NV12)
                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_P010:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            case VA_FOURCC_P016:
#endif
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y16 = (mfxU16*)(pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->U16 = (mfxU16*)(pBuffer + vaapi_mid->m_image.offsets[1]);
                    ptr->V16 = ptr->U16 + 1;
                }
                break;
            case VA_FOURCC_AYUV:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->V = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = ptr->V + 1;
                    ptr->Y = ptr->V + 2;
                    ptr->A = ptr->V + 3;
                }
                break;
#if (MFX_VERSION >= 1027)
            case VA_FOURCC_Y210:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            case VA_FOURCC_Y216:
#endif
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y16 = (mfxU16*)(pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->U16 = ptr->Y16 + 1;
                    ptr->V16 = ptr->Y16 + 3;
                }
                break;
            case VA_FOURCC_Y410:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y410 = (mfxY410*)(pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->Y = 0;
                    ptr->V = 0;
                    ptr->A = 0;
                }
                break;
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            case VA_FOURCC_Y416:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->U16 = (mfxU16*)(pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->Y16 = ptr->U16 + 1;
                    ptr->V16 = ptr->Y16 + 1;
                    ptr->A = (mfxU8*)(ptr->V16 + 1);
                }
                break;
#endif
            default:
                return MFX_ERR_LOCK_MEMORY;
            }
        }

        ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
        ptr->PitchLow = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
    }
    return mfx_res;
}

mfxStatus vaapiFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData* ptr)
{
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;

    if (!vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mid->m_fourcc);

    if (MFX_FOURCC_P8 == mfx_fourcc)   // bitstream processing
    {
        m_libva->vaUnmapBuffer(m_dpy, *(vaapi_mid->m_surface));
    }
    else  // Image processing
    {
        m_libva->vaUnmapBuffer(m_dpy, vaapi_mid->m_image.buf);
        m_libva->vaDestroyImage(m_dpy, vaapi_mid->m_image.image_id);

        if (NULL != ptr)
        {
            ptr->PitchLow = 0;
            ptr->PitchHigh = 0;
            ptr->Y = NULL;
            ptr->U = NULL;
            ptr->V = NULL;
            ptr->A = NULL;
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL* handle)
{
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;

    if (!handle || !vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    *handle = vaapi_mid->m_surface; //VASurfaceID* <-> mfxHDL
    return MFX_ERR_NONE;
}

#endif // #if defined(MFX_VA_LINUX)