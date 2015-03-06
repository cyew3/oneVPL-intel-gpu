/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#ifndef MF_D3D11_COPYSURFACES
#include "d3d11_allocator.h"
#include <vector>
#include <algorithm>

//specific is that this allocator provides only access to texture resource
class MFD3D11FrameAllocator : public D3D11FrameAllocator
{
    typedef D3D11FrameAllocator base;
public:

    // frees memory attached to response
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response)
    {
        if (!m_mids.empty() && response->mids == m_mids.front())
            return MFX_ERR_NONE;
        return base::ReleaseResponse(response);
    }
    // allocates memory
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
    {
        if (request->Type & MFX_MEMTYPE_INTERNAL_FRAME)
            return base::AllocImpl(request, response);

        response->NumFrameActual = request->NumFrameMin;

        m_mids.resize(response->NumFrameActual);
        for (size_t i =0; i < response->NumFrameActual; i++)
        {
            m_mids[i] = (mfxMemId)2000;
        }
        response->mids = response->NumFrameActual?&m_mids.front():NULL;

        return MFX_ERR_NONE;
    }

    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle)
    {
       mfxStatus sts = base::GetFrameHDL(mid, handle);
       if (sts != MFX_ERR_INVALID_HANDLE)
       {
           return sts;
       }
       try
       {
           IMFDXGIBuffer* dxgiBuffer = (IMFDXGIBuffer*)mid;
           //hope dueto ms implementation this interface isn't created dynamically,
           //otherwise we have to create our own interface for holding 2 items: resouce and subresource, instead of using dxgi
           CComPtr<ID3D11Texture2D> p2DTexture;
           HRESULT hr = dxgiBuffer->GetResource(IID_ID3D11Texture2D, (LPVOID*)&p2DTexture);
           if (FAILED(hr) || NULL == p2DTexture)
               return MFX_ERR_UNKNOWN;

           UINT subResIdx = 0;
           hr = dxgiBuffer->GetSubresourceIndex(&subResIdx);
           if (FAILED(hr))
               return MFX_ERR_UNKNOWN;

           mfxHDLPair &hdlPair = (mfxHDLPair&)*handle;
           hdlPair.first = (mfxHDL)p2DTexture;
           hdlPair.second = (mfxHDL)subResIdx;
       }
       catch(...)
       {

       }

       return MFX_ERR_NONE;
    }
    std::vector<mfxMemId> m_mids;
    ptrdiff_t m_idCurrent;
};
#endif