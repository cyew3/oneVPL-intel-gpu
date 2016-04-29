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