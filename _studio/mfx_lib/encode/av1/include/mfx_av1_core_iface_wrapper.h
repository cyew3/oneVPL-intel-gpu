// Copyright (c) 2014-2019 Intel Corporation
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


#pragma once

#include "mfx_common.h"
#include "libmfx_core_interface.h"
#include "libmfx_core.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#define CORE_FUNC_IMPL1(func_name, formal_param_list, actual_param_list) \
static mfxStatus mfxCORE1##func_name formal_param_list \
{ \
    VideoCORE *pCore = (VideoCORE*)pthis; \
    mfxStatus mfxRes; \
    MFX_CHECK(pCore, MFX_ERR_INVALID_HANDLE); \
    try \
    { \
        /* call the method */ \
        mfxRes = pCore->func_name actual_param_list; \
    } \
    /* handle error(s) */ \
    catch(...) \
    { \
        mfxRes = MFX_ERR_NULL_PTR; \
    } \
    return mfxRes; \
} /* mfxStatus mfxCORE1##func_name formal_param_list */

CORE_FUNC_IMPL1(GetHandle, (mfxHDL pthis, mfxHandleType type, mfxHDL *handle), (type, handle))
CORE_FUNC_IMPL1(IncreaseReference, (mfxHDL pthis, mfxFrameData *fd), (fd))
CORE_FUNC_IMPL1(DecreaseReference, (mfxHDL pthis, mfxFrameData *fd), (fd))
CORE_FUNC_IMPL1(CopyFrame, (mfxHDL pthis, mfxFrameSurface1 *dst, mfxFrameSurface1 *src), (dst, src))
CORE_FUNC_IMPL1(CopyBuffer, (mfxHDL pthis, mfxU8 *dst, mfxU32 dst_size, mfxFrameSurface1 *src), (dst, dst_size, src))

#undef CORE_FUNC_IMPL1

static mfxStatus mfxCOREGetCoreParam1(mfxHDL pthis, mfxCoreParam *par)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    // reset the parameters
    memset(par, 0, sizeof(mfxCoreParam));

    eMFXVAType impl = pCore->GetVAType();
    if (impl == MFX_HW_D3D9)
        par->Impl = MFX_IMPL_VIA_D3D9;
    else if (impl == MFX_HW_D3D11)
        par->Impl = MFX_IMPL_VIA_D3D11;
    else if (impl == MFX_IMPL_VIA_VAAPI)
        par->Impl = MFX_HANDLE_VA_DISPLAY;
    else par->Impl = 0;

    par->Version.Major = MFX_VERSION_MAJOR;
    par->Version.Minor = MFX_VERSION_MINOR;
    par->NumWorkingThread = pCore->GetNumWorkingThreads();

    return mfxRes;

} // mfxStatus mfxCOREGetCoreParam(mfxHDL pthis, mfxCoreParam *par)

static mfxStatus mfxCOREGetFrameHDL1(mfxHDL pthis, mfxFrameData *fd, mfxHDL *handle)
{
    VideoCORE* pCore = (VideoCORE*)pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(pCore, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(handle);

    try
    {
        if (pCore->IsExternalFrameAllocator()
            && !(fd->MemType & MFX_MEMTYPE_OPAQUE_FRAME))
        {
            mfxRes = pCore->GetExternalFrameHDL(fd->MemId, handle);
        }
        else
        {
            mfxRes = pCore->GetFrameHDL(fd->MemId, handle);
        }
    }
    catch (...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
} // mfxStatus mfxCOREGetFrameHDL(mfxHDL pthis, mfxFrameData *fd, mfxHDL *handle)

static mfxStatus mfxCOREGetRealSurface1(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf)
{
    VideoCORE* pCore = (VideoCORE*)pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_CHECK(pCore, MFX_ERR_NOT_INITIALIZED);

    try
    {
        *surf = pCore->GetNativeSurface(op_surf);
        if (!*surf)
            return MFX_ERR_INVALID_HANDLE;

        return mfxRes;
    }
    // handle error(s)
    catch (...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
} // mfxStatus mfxCOREGetRealSurface(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf)

static mfxStatus mfxCORECreateAccelerationDevice1(mfxHDL pthis, mfxHandleType type, mfxHDL *handle)
{
    VideoCORE* pCore = (VideoCORE*)pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(pCore, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(handle);
    try
    {
        mfxRes = pCore->GetHandle(type, handle);

        if (mfxRes == MFX_ERR_NOT_FOUND)
        {
#if defined(MFX_VA_WIN)
            if (type == MFX_HANDLE_D3D9_DEVICE_MANAGER)
            {
                D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(pCore, MFXICORED3D_GUID);
                if (pID3D == 0)
                    mfxRes = MFX_ERR_UNSUPPORTED;
                else
                {
                    IDirectXVideoDecoderService *service = 0;
                    mfxRes = pID3D->GetD3DService(1920, 1088, &service);

                    *handle = (mfxHDL)pID3D->GetD3D9DeviceManager();
                }
            }
            else if (type == MFX_HANDLE_D3D11_DEVICE)
            {
                D3D11Interface* pID3D = QueryCoreInterface<D3D11Interface>(pCore);
                if (pID3D == 0)
                    mfxRes = MFX_ERR_UNSUPPORTED;
                else
                {
                    *handle = (mfxHDL)pID3D->GetD3D11Device();
                    if (*handle)
                        mfxRes = MFX_ERR_NONE;
                }
            }
            else
#endif
            {
                mfxRes = MFX_ERR_UNSUPPORTED;
            }
        }
    }
    /* handle error(s) */
    catch (...)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }
    return mfxRes;
}// mfxStatus mfxCORECreateAccelerationDevice(mfxHDL pthis, mfxHandleType type, mfxHDL *handle)

static mfxStatus mfxCOREQueryPlatform1(mfxHDL pthis, mfxPlatform *platform)
{
    VideoCORE* pCore = (VideoCORE*)pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(pCore, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(platform);

    try
    {
        IVideoCore_API_1_19 * pInt = QueryCoreInterface<IVideoCore_API_1_19>(pCore, MFXICORE_API_1_19_GUID);
        if (pInt)
        {
            mfxRes = pInt->QueryPlatform(platform);
        }
        else
        {
            mfxRes = MFX_ERR_UNSUPPORTED;
            memset(platform, 0, sizeof(mfxPlatform));
        }
    }
    catch (...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
} // mfxCOREQueryPlatform(mfxHDL pthis, mfxPlatform *platform)

// exposed default allocator
static mfxStatus mfxDefAllocFrames1(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    mfxFrameAllocator* pExtAlloc = (mfxFrameAllocator*)pCore->QueryCoreInterface(MFXIEXTERNALLOC_GUID);
    return pExtAlloc ? pExtAlloc->Alloc(pExtAlloc->pthis, request, response) : pCore->AllocFrames(request, response);

} // mfxStatus mfxDefAllocFrames1(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)

static mfxStatus mfxDefLockFrame1(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;

    if (pCore->IsExternalFrameAllocator())
    {
        return pCore->LockExternalFrame(mid, ptr);
    }

    return pCore->LockFrame(mid, ptr);


} // mfxStatus mfxDefLockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
static mfxStatus mfxDefGetHDL1(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    if (pCore->IsExternalFrameAllocator())
    {
        return pCore->GetExternalFrameHDL(mid, handle);
    }
    return pCore->GetFrameHDL(mid, handle);

} // mfxStatus mfxDefGetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
static mfxStatus mfxDefUnlockFrame1(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr = 0)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;

    if (pCore->IsExternalFrameAllocator())
    {
        return pCore->UnlockExternalFrame(mid, ptr);
    }

    return pCore->UnlockFrame(mid, ptr);

} // mfxStatus mfxDefUnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0)
static mfxStatus mfxDefFreeFrames1(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    mfxFrameAllocator* pExtAlloc = (mfxFrameAllocator*)pCore->QueryCoreInterface(MFXIEXTERNALLOC_GUID);
    return pExtAlloc ? pExtAlloc->Free(pExtAlloc->pthis, response) : pCore->FreeFrames(response);

} // mfxStatus mfxDefFreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response)

static void InitCoreInterface1(mfxCoreInterface *pCoreInterface, const VideoCORE *core)
{
    // reset the structure
    memset(pCoreInterface, 0, sizeof(mfxCoreInterface));


    // fill external allocator
    pCoreInterface->FrameAllocator.pthis = (mfxHDL)core;
    pCoreInterface->FrameAllocator.Alloc = &mfxDefAllocFrames1;
    pCoreInterface->FrameAllocator.Lock = &mfxDefLockFrame1;
    pCoreInterface->FrameAllocator.GetHDL = &mfxDefGetHDL1;
    pCoreInterface->FrameAllocator.Unlock = &mfxDefUnlockFrame1;
    pCoreInterface->FrameAllocator.Free = &mfxDefFreeFrames1;

    // fill the methods
    pCoreInterface->pthis = (mfxHDL)core;
    pCoreInterface->GetCoreParam = &mfxCOREGetCoreParam1;
    pCoreInterface->GetHandle = &mfxCORE1GetHandle;
    pCoreInterface->GetFrameHandle = &mfxCOREGetFrameHDL1;
    pCoreInterface->IncreaseReference = &mfxCORE1IncreaseReference;
    pCoreInterface->DecreaseReference = &mfxCORE1DecreaseReference;
    pCoreInterface->CopyFrame = &mfxCORE1CopyFrame;
    pCoreInterface->CopyBuffer = &mfxCORE1CopyBuffer;
    pCoreInterface->GetRealSurface = &mfxCOREGetRealSurface1;
    pCoreInterface->CreateAccelerationDevice = &mfxCORECreateAccelerationDevice1;
    pCoreInterface->QueryPlatform = &mfxCOREQueryPlatform1;

} // void InitCoreInterface1(mfxCoreInterface *pCoreInterface,

class MFXCoreInterface1
{
public:
    mfxCoreInterface m_core;
public:
    MFXCoreInterface1()
        : m_core() {
    }
    MFXCoreInterface1(const mfxCoreInterface & pCore)
        : m_core(pCore) {
    }
    MFXCoreInterface1(const VideoCORE *pCore)
    {
        InitCoreInterface1(&m_core, pCore);
    }

    MFXCoreInterface1(const MFXCoreInterface1 & that)
        : m_core(that.m_core) {
    }
    MFXCoreInterface1 &operator = (const MFXCoreInterface1 & that)
    {
        m_core = that.m_core;
        return *this;
    }
    virtual bool IsCoreSet() {
        return m_core.pthis != 0;
    }
    virtual mfxStatus GetCoreParam(mfxCoreParam *par) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetCoreParam(m_core.pthis, par);
    }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *handle) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetHandle(m_core.pthis, type, handle);
    }
    virtual mfxStatus IncreaseReference(mfxFrameData *fd) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.IncreaseReference(m_core.pthis, fd);
    }
    virtual mfxStatus DecreaseReference(mfxFrameData *fd) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.DecreaseReference(m_core.pthis, fd);
    }
    virtual mfxStatus CopyFrame(mfxFrameSurface1 *dst, mfxFrameSurface1 *src) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.CopyFrame(m_core.pthis, dst, src);
    }
    virtual mfxStatus CopyBuffer(mfxU8 *dst, mfxU32 size, mfxFrameSurface1 *src) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.CopyBuffer(m_core.pthis, dst, size, src);
    }
    virtual mfxStatus MapOpaqueSurface(mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.MapOpaqueSurface(m_core.pthis, num, type, op_surf);
    }
    virtual mfxStatus UnmapOpaqueSurface(mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.UnmapOpaqueSurface(m_core.pthis, num, type, op_surf);
    }
    virtual mfxStatus GetRealSurface(mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetRealSurface(m_core.pthis, op_surf, surf);
    }
    virtual mfxStatus CreateAccelerationDevice(mfxHandleType type, mfxHDL *handle) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.CreateAccelerationDevice(m_core.pthis, type, handle);
    }
    virtual mfxFrameAllocator & FrameAllocator() {
        return m_core.FrameAllocator;
    }
};

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE

