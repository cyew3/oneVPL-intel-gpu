// Copyright (c) 2007-2019 Intel Corporation
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

#include "mfx_common.h"
#if defined (MFX_VA_WIN)

#include "mfxvideo++int.h"
#include "libmfx_allocator_d3d9.h"

#include "ippcore.h"
#include "ipps.h"

#include "mfx_utils.h"

#define D3DFMT_NV12     (D3DFORMAT)MAKEFOURCC('N','V','1','2')
#define D3DFMT_P010     (D3DFORMAT)MAKEFOURCC('P','0','1','0')
#define D3DFMT_YV12     (D3DFORMAT)MAKEFOURCC('Y','V','1','2')
#define D3DFMT_IMC3     (D3DFORMAT)MAKEFOURCC('I','M','C','3')
#define D3DFMT_YUV400   (D3DFORMAT)MAKEFOURCC('4','0','0','P')
#define D3DFMT_YUV411   (D3DFORMAT)MAKEFOURCC('4','1','1','P')
#define D3DFMT_YUV422H  (D3DFORMAT)MAKEFOURCC('4','2','2','H')
#define D3DFMT_YUV422V  (D3DFORMAT)MAKEFOURCC('4','2','2','V')
#define D3DFMT_YUV444   (D3DFORMAT)MAKEFOURCC('4','4','4','P')
#define D3DFMT_AYUV     (D3DFORMAT)MFX_FOURCC_AYUV

#define D3DFMT_Y410     (D3DFORMAT)MFX_FOURCC_Y410
#define D3DFMT_Y210     (D3DFORMAT)MFX_FOURCC_Y210

#define D3DFMT_P016     (D3DFORMAT)MFX_FOURCC_P016
#define D3DFMT_Y216     (D3DFORMAT)MFX_FOURCC_Y216
#define D3DFMT_Y416     (D3DFORMAT)MFX_FOURCC_Y416

// D3D9 surface working
mfxStatus mfxDefaultAllocatorD3D9::AllocFramesHW(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    // only NV12 and D3DFMT_P8 buffers are supported by HW
    switch(request->Info.FourCC)
    {
    case MFX_FOURCC_NV12:
    case D3DFMT_P8:
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_IMC3:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_BGR4:
    case MFX_FOURCC_YUV400:
    case MFX_FOURCC_YUV411:
    case MFX_FOURCC_YUV422H:
    case MFX_FOURCC_YUV422V:
    case MFX_FOURCC_YUV444:
    case MFX_FOURCC_RGBP:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_A2RGB10:
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_P016:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_Y416:
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    mfxU16 numAllocated, maxNumFrames;
    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;

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
    mfxU32  fourcc = request->Info.FourCC;

    if (MFX_FOURCC_RGB4 == fourcc)
    {
        fourcc = D3DFMT_A8R8G8B8;
    }
    else if (MFX_FOURCC_BGR4 == fourcc)
    {
        fourcc = D3DFMT_A8B8G8R8;
    }
    else if (MFX_FOURCC_A2RGB10 == fourcc)
    {
        fourcc = D3DFMT_A2R10G10B10;
    }

    DWORD   target;
    HRESULT hr;

    if (MFX_MEMTYPE_DXVA2_DECODER_TARGET & request->Type)
    {
        target = DXVA2_VideoDecoderRenderTarget;
    }
    else if (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET & request->Type)
    {
        target = DXVA2_VideoProcessorRenderTarget;
    }
    else
        return MFX_ERR_INVALID_HANDLE;

    // allocate frames in cycle
    maxNumFrames = request->NumFrameSuggested;

    IDirect3DSurface9 ** pSurfaces = new IDirect3DSurface9*[maxNumFrames];

    hr = pSelf->pDirectXVideoService->CreateSurface(width,
                                                    height,
                                                    maxNumFrames - 1, //number of BackBuffer
                                                    (D3DFORMAT)fourcc,
                                                    D3DPOOL_DEFAULT,
                                                    0,
                                                    target,
                                                    pSurfaces,
                                                    NULL);

    if (SUCCEEDED(hr))
    {
        pSelf->m_frameHandles.resize(maxNumFrames);

        for (numAllocated = 0; numAllocated < maxNumFrames; numAllocated++)
        {
            pSelf->m_SrfQueue.push_back(pSurfaces[numAllocated]);
            pSelf->m_frameHandles[numAllocated] = (mfxHDL)pSelf->m_SrfQueue.size();
        }
        response->mids = &pSelf->m_frameHandles[0];
        response->NumFrameActual = maxNumFrames;
        pSelf->NumFrames = maxNumFrames;
    }
    else
    {
        delete[] pSurfaces;
        return MFX_ERR_MEMORY_ALLOC;
    }
    delete[] pSurfaces;

    // check the number of allocated frames
    if (numAllocated < request->NumFrameMin)
    {
        FreeFramesHW(pthis, response);
        return MFX_ERR_MEMORY_ALLOC;
    }

    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorD3D9::ReallocFrameHW(mfxHDL pthis, const mfxMemId mid, const mfxFrameInfo * info, const mfxU16 MemType)
{
    MFX_CHECK(info, MFX_ERR_NULL_PTR);
    MFX_CHECK(pthis, MFX_ERR_NULL_PTR);
    MFX_CHECK(mid, MFX_ERR_NULL_PTR);

    // only NV12 and D3DFMT_P8 buffers are supported by HW
    switch (info->FourCC)
    {
    case MFX_FOURCC_NV12:
    case D3DFMT_P8:
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_IMC3:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_BGR4:
    case MFX_FOURCC_YUV400:
    case MFX_FOURCC_YUV411:
    case MFX_FOURCC_YUV422H:
    case MFX_FOURCC_YUV422V:
    case MFX_FOURCC_YUV444:
    case MFX_FOURCC_RGBP:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_A2RGB10:
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_P016:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_Y416:
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    HRESULT hRes;

    UINT    width  = info->Width;
    UINT    height = info->Height;
    mfxU32  fourcc = info->FourCC;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    if (!pSelf)
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);


    if (MFX_FOURCC_RGB4 == fourcc)
    {
        fourcc = D3DFMT_A8R8G8B8;
    }
    else if (MFX_FOURCC_BGR4 == fourcc)
    {
        fourcc = D3DFMT_A8B8G8R8;
    }
    else if (MFX_FOURCC_A2RGB10 == fourcc)
    {
        fourcc = D3DFMT_A2R10G10B10;
    }

    DWORD   target;

    if (MFX_MEMTYPE_DXVA2_DECODER_TARGET & MemType)
    {
        target = DXVA2_VideoDecoderRenderTarget;
    }
    else if (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET & MemType)
    {
        target = DXVA2_VideoProcessorRenderTarget;
    }
    else
        return MFX_ERR_INVALID_HANDLE;


    IDirect3DSurface9 * pSurface;

    
    size_t index = (size_t)mid - 1;
    if (pSelf->m_SrfQueue.size() <= index)
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    //release surface that need to reallocate
    pSurface = pSelf->m_SrfQueue[index];
    pSurface->Release();

    //reallocate realeased surface with new info
    hRes = pSelf->pDirectXVideoService->CreateSurface(width,
                                                      height,
                                                      0, //number of BackBuffer
                                                      (D3DFORMAT)fourcc,
                                                      D3DPOOL_DEFAULT,
                                                      0,
                                                      target,
                                                      &pSurface,
                                                      NULL);
    if (FAILED(hRes))
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }
    pSelf->m_SrfQueue[index] = pSurface;
    return MFX_ERR_NONE;
}



mfxStatus mfxDefaultAllocatorD3D9::SetFrameData(const D3DSURFACE_DESC &desc, const D3DLOCKED_RECT &LockedRect, mfxFrameData *ptr)
{
    switch ((DWORD)desc.Format)
    {
    case D3DFMT_P010:
    case D3DFMT_P016:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->U = (mfxU8 *)LockedRect.pBits + desc.Height * LockedRect.Pitch;
        ptr->V = ptr->U + 1;
        break;
    case D3DFMT_NV12:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->U = (mfxU8 *)LockedRect.pBits + desc.Height * LockedRect.Pitch;
        ptr->V = ptr->U + 1;
        break;
    case D3DFMT_YV12:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->V = ptr->Y + desc.Height * LockedRect.Pitch;
        ptr->U = ptr->V + (desc.Height * LockedRect.Pitch) / 4;
        break;
    case D3DFMT_YUY2:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        break;
    case D3DFMT_R8G8B8:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->B = (mfxU8 *)LockedRect.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        break;
    case D3DFMT_A8R8G8B8:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->B = (mfxU8 *)LockedRect.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
    case D3DFMT_A8B8G8R8:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->R = (mfxU8 *)LockedRect.pBits;
        ptr->G = ptr->R + 1;
        ptr->B = ptr->R + 2;
        ptr->A = ptr->R + 3;
        break;
    case D3DFMT_P8:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->U = 0;
        ptr->V = 0;
        break;
    case D3DFMT_IMC3:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->U = ptr->Y + desc.Height * LockedRect.Pitch;
        ptr->V = ptr->U + (desc.Height * LockedRect.Pitch) / 2;
        break;
    case D3DFMT_YUV400:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->V = ptr->Y + desc.Height * LockedRect.Pitch;
        ptr->U = ptr->V + (desc.Height * LockedRect.Pitch) / 4;
        break;
    case D3DFMT_YUV411:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->U = ptr->Y + desc.Height * LockedRect.Pitch;
        ptr->V = ptr->U + desc.Height * LockedRect.Pitch / 4;
        break;
    case D3DFMT_YUV422H:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->U = ptr->Y + desc.Height * LockedRect.Pitch;
        ptr->V = ptr->U + desc.Height * LockedRect.Pitch / 2;
        break;
    case D3DFMT_YUV422V:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->U = ptr->Y + desc.Height * LockedRect.Pitch;
        ptr->V = ptr->U + desc.Height / 2 * LockedRect.Pitch;
        break;
    case D3DFMT_YUV444:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pBits;
        ptr->U = ptr->Y + desc.Height * LockedRect.Pitch;
        ptr->V = ptr->U + desc.Height * LockedRect.Pitch;
        break;
    case D3DFMT_AYUV:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->B = (mfxU8 *)LockedRect.pBits;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
    case D3DFMT_A2R10G10B10:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->R = ptr->G = ptr->B = ptr->A = (mfxU8 *)LockedRect.pBits;
        break;

    case D3DFMT_Y210:
    case D3DFMT_Y216:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y16 = (mfxU16*)LockedRect.pBits;
        ptr->U16 = ptr->Y16 + 1;
        ptr->V16 = ptr->Y16 + 3;
        break;
    case D3DFMT_Y410:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->Y410 = (mfxY410 *)LockedRect.pBits;
        ptr->Y = 0;
        ptr->V = 0;
        ptr->A = 0;
        break;

    case D3DFMT_Y416:
        ptr->PitchHigh = (mfxU16)(LockedRect.Pitch / (1 << 16));
        ptr->PitchLow = (mfxU16)(LockedRect.Pitch % (1 << 16));
        ptr->U16 = (mfxU16*)LockedRect.pBits;
        ptr->Y16 = ptr->U16 + 1;
        ptr->V16 = ptr->Y16 + 1;
        ptr->A   = (mfxU8 *)(ptr->V16 + 1);
        break;

    default:
        return MFX_ERR_LOCK_MEMORY;
    }

    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorD3D9::LockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    HRESULT hr = S_OK;
    // TBD
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    size_t index =  (size_t)mid - 1;

    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT LockedRect;
    IDirect3DSurface9    *RenderTarget = pSelf->m_SrfQueue[index];
    // Get surface description
    hr = RenderTarget->GetDesc(&desc);
    if (S_OK != hr)
        return MFX_ERR_LOCK_MEMORY;
    // Lock surface
    hr = RenderTarget->LockRect(&LockedRect, NULL, D3DLOCK_NOSYSLOCK );
    if (S_OK !=hr)
        return MFX_ERR_LOCK_MEMORY;

    return SetFrameData(desc, LockedRect, ptr);
}

mfxStatus mfxDefaultAllocatorD3D9::GetHDLHW(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    if (0 == mid)
        return MFX_ERR_INVALID_HANDLE;

    size_t index =  (size_t)mid - 1;

    if (index >= pSelf->m_SrfQueue.size())
        return MFX_ERR_INVALID_HANDLE;

    *handle = pSelf->m_SrfQueue[index];
    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorD3D9::UnlockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr)
{
    // TBD
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    size_t index =  (size_t)mid - 1;
    IDirect3DSurface9    *RenderTarget = pSelf->m_SrfQueue[index];
    RenderTarget->UnlockRect();

    if (ptr) {
        ptr->PitchHigh=0;
        ptr->PitchLow=0;
        ptr->U=ptr->V=ptr->Y=0;
        ptr->A=ptr->R=ptr->G=ptr->B=0;
    }
    return MFX_ERR_NONE;
}
mfxStatus mfxDefaultAllocatorD3D9::FreeFramesHW(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    mfxU32 numAllocated;
    IDirect3DSurface9 *pSurface;
    size_t Index;

    mfxStatus sts = MFX_ERR_NONE;
    for (numAllocated = 0; numAllocated < response->NumFrameActual; numAllocated++)
    {
        Index = (size_t)response->mids[numAllocated] - 1;
        if (Index > pSelf->m_SrfQueue.size())
        {
            sts = MFX_ERR_INVALID_HANDLE;
            continue;
        }

        pSurface = pSelf->m_SrfQueue[Index];
        pSurface->Release();
    }

    pSelf->m_frameHandles.clear();

    // TBD
    return sts;
}

mfxDefaultAllocatorD3D9::mfxWideHWFrameAllocator::mfxWideHWFrameAllocator(mfxU16 type,
                                                                          IDirectXVideoDecoderService *pExtDirectXVideoService):mfxBaseWideFrameAllocator(type),
                                                                                                                               pDirectXVideoService(pExtDirectXVideoService)
{
    frameAllocator.Alloc = &mfxDefaultAllocatorD3D9::AllocFramesHW;
    frameAllocator.Lock = &mfxDefaultAllocatorD3D9::LockFrameHW;
    frameAllocator.GetHDL = &mfxDefaultAllocatorD3D9::GetHDLHW;
    frameAllocator.Unlock = &mfxDefaultAllocatorD3D9::UnlockFrameHW;
    frameAllocator.Free = &mfxDefaultAllocatorD3D9::FreeFramesHW;
}


#endif // ifdef MFX_VA_WIN



