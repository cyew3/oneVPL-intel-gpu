//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#include <mfx_session.h>

#include <vm_time.h>
#include <vm_sys_info.h>

#include <libmfx_core_factory.h>
#include <libmfx_core.h>

#if defined (MFX_VA_WIN)
#include <libmfx_core_d3d9.h>
#include <atlbase.h>
#elif defined(MFX_VA_LINUX)
#include <libmfx_core_vaapi.h>
#endif


// static section of the file
namespace
{

mfxStatus mfxCOREGetCoreParam(mfxHDL pthis, mfxCoreParam *par)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    try
    {
        MFX_SCHEDULER_PARAM param;

        // reset the parameters
        memset(par, 0, sizeof(mfxCoreParam));

        // get the parameters of the current scheduler
        mfxRes = session->m_pScheduler->GetParam(&param);
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }

        // fill the structure
        mfxRes = MFXQueryIMPL(session, &(par->Impl));
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }
        par->Version = session->m_version;
        par->NumWorkingThread = param.numberOfThreads;
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pScheduler)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
        else if (0 == par)
        {
            mfxRes = MFX_ERR_NULL_PTR;
        }
    }

    return mfxRes;

} // mfxStatus mfxCOREGetCoreParam(mfxHDL pthis, mfxCoreParam *par)

mfxStatus mfxCOREMapOpaqueSurface(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);

    CommonCORE *pCore = (CommonCORE *)session->m_pCORE->QueryCoreInterface(MFXIVideoCORE_GUID);
    if (!pCore)
        return MFX_ERR_INVALID_HANDLE;
    
    try
    {
        if (!op_surf)
            return MFX_ERR_MEMORY_ALLOC;
        
        if (!*op_surf)
            return MFX_ERR_MEMORY_ALLOC;

        mfxFrameAllocRequest  request;
        mfxFrameAllocResponse response;

        request.Type =        (mfxU16)type;
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)num;
        request.Info = op_surf[0]->Info;

        mfxRes = pCore->AllocFrames(&request, &response, op_surf, num);
        MFX_CHECK_STS(mfxRes);

        pCore->AddPluginAllocResponse(response);

        return mfxRes;

    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pScheduler)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
    }

    return mfxRes;

} // mfxStatus mfxCOREMapOpaqueSurface(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf)
mfxStatus mfxCOREUnmapOpaqueSurface(mfxHDL pthis, mfxU32  num, mfxU32  , mfxFrameSurface1 **op_surf)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);

    CommonCORE *pCore = (CommonCORE *)session->m_pCORE->QueryCoreInterface(MFXIVideoCORE_GUID);
    if (!pCore)
        return MFX_ERR_INVALID_HANDLE;

    try
    {
        if (!op_surf)
            return MFX_ERR_MEMORY_ALLOC;

        if (!*op_surf)
            return MFX_ERR_MEMORY_ALLOC;

        mfxFrameAllocResponse response;
        mfxFrameSurface1 *pSurf = NULL;

        std::vector<mfxMemId> mids(num);
        response.mids = &mids[0];
        response.NumFrameActual = (mfxU16) num;
        for (mfxU32 i=0; i < num; i++)
        {
            pSurf = pCore->GetNativeSurface(op_surf[i]);
            if (!pSurf)
                return MFX_ERR_INVALID_HANDLE;

            response.mids[i] = pSurf->Data.MemId;
        }

        if (!pCore->GetPluginAllocResponse(response))
            return MFX_ERR_INVALID_HANDLE;

        mfxRes = session->m_pCORE->FreeFrames(&response);
        return mfxRes;

    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pScheduler)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
    }

    return mfxRes;

} // mfxStatus mfxCOREUnmapOpaqueSurface(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf)

mfxStatus mfxCOREGetRealSurface(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);

    try
    {
        *surf = session->m_pCORE->GetNativeSurface(op_surf);
        if (!*surf)
            return MFX_ERR_INVALID_HANDLE;

        return mfxRes;
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pScheduler)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
    }

    return mfxRes;
} // mfxStatus mfxCOREGetRealSurface(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf)

mfxStatus mfxCOREGetOpaqueSurface(mfxHDL pthis, mfxFrameSurface1 *surf, mfxFrameSurface1 **op_surf)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);
    
    try
    {
        *op_surf = session->m_pCORE->GetOpaqSurface(surf->Data.MemId);
        if (!*op_surf)
            return MFX_ERR_INVALID_HANDLE;

        return mfxRes;
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pScheduler)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
    }

    return mfxRes;
}// mfxStatus mfxCOREGetOpaqueSurface(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf)

mfxStatus mfxCORECreateAccelerationDevice(mfxHDL pthis, mfxHandleType type, mfxHDL *handle)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(handle);
    try
    {
        mfxRes = session->m_pCORE.get()->GetHandle(type, handle);

        if (mfxRes == MFX_ERR_NOT_FOUND)
        {
#if defined(MFX_VA_WIN)
            if (type == MFX_HANDLE_D3D9_DEVICE_MANAGER)
            {
                D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(session->m_pCORE.get(), MFXICORED3D_GUID);
                if(pID3D == 0) 
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
                D3D11Interface* pID3D = QueryCoreInterface<D3D11Interface>(session->m_pCORE.get());
                if(pID3D == 0) 
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
    catch(MFX_CORE_CATCH_TYPE)
    {
        /* set the default error value */
        if (NULL == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (NULL == session->m_pCORE.get())
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
        else
        {
            mfxRes = MFX_ERR_NULL_PTR;
        }
    }
    return mfxRes;
}// mfxStatus mfxCORECreateAccelerationDevice(mfxHDL pthis, mfxHandleType type, mfxHDL *handle)

mfxStatus mfxCOREGetFrameHDL(mfxHDL pthis, mfxFrameData *fd, mfxHDL *handle)
{
    mfxSession session = (mfxSession)pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(handle);
    VideoCORE *pCore = session->m_pCORE.get();

    try
    {
        if (   pCore->IsExternalFrameAllocator()
            && !(fd->MemType & MFX_MEMTYPE_OPAQUE_FRAME))
        {
            mfxRes = pCore->GetExternalFrameHDL(fd->MemId, handle);
        }
        else
        {
            mfxRes = pCore->GetFrameHDL(fd->MemId, handle);
        }
    }
    catch (MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
} // mfxStatus mfxCOREGetFrameHDL(mfxHDL pthis, mfxFrameData *fd, mfxHDL *handle)

mfxStatus mfxCOREQueryPlatform(mfxHDL pthis, mfxPlatform *platform)
{
    mfxSession session = (mfxSession)pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(platform);
    VideoCORE *pCore = session->m_pCORE.get();

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
    catch (MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
} // mfxCOREQueryPlatform(mfxHDL pthis, mfxPlatform *platform)

#define CORE_FUNC_IMPL(func_name, formal_param_list, actual_param_list) \
mfxStatus mfxCORE##func_name formal_param_list \
{ \
    mfxSession session = (mfxSession) pthis; \
    mfxStatus mfxRes; \
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE); \
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED); \
    try \
    { \
        /* call the method */ \
        mfxRes = session->m_pCORE->func_name actual_param_list; \
    } \
    /* handle error(s) */ \
    catch(MFX_CORE_CATCH_TYPE) \
    { \
        /* set the default error value */ \
        if (NULL == session) \
        { \
            mfxRes = MFX_ERR_INVALID_HANDLE; \
        } \
        else if (NULL == session->m_pCORE.get()) \
        { \
            mfxRes = MFX_ERR_NOT_INITIALIZED; \
        } \
        else \
        { \
            mfxRes = MFX_ERR_NULL_PTR; \
        } \
    } \
    return mfxRes; \
} /* mfxStatus mfxCORE##func_name formal_param_list */

CORE_FUNC_IMPL(GetHandle, (mfxHDL pthis, mfxHandleType type, mfxHDL *handle), (type, handle))
CORE_FUNC_IMPL(IncreaseReference, (mfxHDL pthis, mfxFrameData *fd), (fd))
CORE_FUNC_IMPL(DecreaseReference, (mfxHDL pthis, mfxFrameData *fd), (fd))
CORE_FUNC_IMPL(CopyFrame, (mfxHDL pthis, mfxFrameSurface1 *dst, mfxFrameSurface1 *src), (dst, src))
CORE_FUNC_IMPL(CopyBuffer, (mfxHDL pthis, mfxU8 *dst, mfxU32 dst_size, mfxFrameSurface1 *src), (dst, dst_size, src))
CORE_FUNC_IMPL(CopyFrameEx, (mfxHDL pthis, mfxFrameSurface1 *dst, mfxU16  dstMemType, mfxFrameSurface1 *src, mfxU16  srcMemType), (dst, dstMemType, src, srcMemType))

#undef CORE_FUNC_IMPL

// exposed default allocator
mfxStatus mfxDefAllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    mfxFrameAllocator* pExtAlloc = (mfxFrameAllocator*)pCore->QueryCoreInterface(MFXIEXTERNALLOC_GUID);
    return pExtAlloc?pExtAlloc->Alloc(pExtAlloc->pthis,request,response):pCore->AllocFrames(request,response);

} // mfxStatus mfxDefAllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
mfxStatus mfxDefLockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;

    if (pCore->IsExternalFrameAllocator())
    {
        return pCore->LockExternalFrame(mid,ptr); 
    }

    return pCore->LockFrame(mid,ptr); 


} // mfxStatus mfxDefLockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
mfxStatus mfxDefGetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    if (pCore->IsExternalFrameAllocator())
    {
        return pCore->GetExternalFrameHDL(mid, handle);
    }
    return pCore->GetFrameHDL(mid, handle);

} // mfxStatus mfxDefGetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
mfxStatus mfxDefUnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    
    if (pCore->IsExternalFrameAllocator())
    {
        return pCore->UnlockExternalFrame(mid, ptr);
    }
    
    return pCore->UnlockFrame(mid, ptr); 

} // mfxStatus mfxDefUnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0)
mfxStatus mfxDefFreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    mfxFrameAllocator* pExtAlloc = (mfxFrameAllocator*)pCore->QueryCoreInterface(MFXIEXTERNALLOC_GUID);
    return pExtAlloc?pExtAlloc->Free(pExtAlloc->pthis,response):pCore->FreeFrames(response); 

} // mfxStatus mfxDefFreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response)



// exposed external allocator
mfxStatus mfxExtAllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    MFX_CHECK_NULL_PTR1(pthis);
    if (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        VideoCORE *pCore = (VideoCORE*)pthis;
        return pCore->AllocFrames(request,response); 
    }
    return MFX_ERR_UNSUPPORTED;

} // mfxStatus mfxExtAllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
mfxStatus mfxExtLockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    VideoCORE *pCore = (VideoCORE*)pthis;
    return pCore->LockExternalFrame(mid,ptr); 

} // mfxStatus mfxExtLockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
mfxStatus mfxExtGetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    VideoCORE *pCore = (VideoCORE*)pthis;
    return pCore->GetExternalFrameHDL(mid, handle);

} // mfxStatus mfxExtGetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
mfxStatus mfxExtUnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0)
{
    VideoCORE *pCore = (VideoCORE*)pthis;
    return pCore->UnlockExternalFrame(mid, ptr); 

} // mfxStatus mfxExtUnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0)
mfxStatus mfxExtFreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    VideoCORE *pCore = (VideoCORE*)pthis;
    return pCore->FreeFrames(response); 

} // mfxStatus mfxExtFreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response)

void InitCoreInterface(mfxCoreInterface *pCoreInterface,
                       const mfxSession session)
{
    // reset the structure
    memset(pCoreInterface, 0, sizeof(mfxCoreInterface));


     // fill external allocator
    pCoreInterface->FrameAllocator.pthis = session->m_pCORE.get();
    pCoreInterface->FrameAllocator.Alloc = &mfxDefAllocFrames;
    pCoreInterface->FrameAllocator.Lock = &mfxDefLockFrame;
    pCoreInterface->FrameAllocator.GetHDL = &mfxDefGetHDL;
    pCoreInterface->FrameAllocator.Unlock = &mfxDefUnlockFrame;
    pCoreInterface->FrameAllocator.Free = &mfxDefFreeFrames;

    // fill the methods
    pCoreInterface->pthis = (mfxHDL) session;
    pCoreInterface->GetCoreParam = &mfxCOREGetCoreParam;
    pCoreInterface->GetHandle = &mfxCOREGetHandle;
    pCoreInterface->GetFrameHandle = &mfxCOREGetFrameHDL;
    pCoreInterface->IncreaseReference = &mfxCOREIncreaseReference;
    pCoreInterface->DecreaseReference = &mfxCOREDecreaseReference;
    pCoreInterface->CopyFrame = &mfxCORECopyFrame;
    pCoreInterface->CopyBuffer = &mfxCORECopyBuffer;
    pCoreInterface->MapOpaqueSurface = &mfxCOREMapOpaqueSurface;
    pCoreInterface->UnmapOpaqueSurface = &mfxCOREUnmapOpaqueSurface;
    pCoreInterface->GetRealSurface = &mfxCOREGetRealSurface;
    pCoreInterface->GetOpaqueSurface = &mfxCOREGetOpaqueSurface;
    pCoreInterface->CreateAccelerationDevice = &mfxCORECreateAccelerationDevice;
    pCoreInterface->QueryPlatform = &mfxCOREQueryPlatform;

} // void InitCoreInterface(mfxCoreInterface *pCoreInterface,

} // namespace


//////////////////////////////////////////////////////////////////////////
//  Global free functions
//////////////////////////////////////////////////////////////////////////

MFXIPtr<MFXISession_1_10> TryGetSession_1_10(mfxSession session)
{
    MFXIPtr<MFXISession_1_10> nullPtr;
    if (session == NULL)
    {
        return nullPtr;
    }

    if (session->m_version.Major != 1 || session->m_version.Minor < 10)
    {
        return nullPtr;
    }

    _mfxSession_1_10 * newPtr = (_mfxSession_1_10 *)session;

    return MFXIPtr<MFXISession_1_10>( newPtr->QueryInterface(MFXISession_1_10_GUID) );
}

//////////////////////////////////////////////////////////////////////////
//  _mfxSession members
//////////////////////////////////////////////////////////////////////////

_mfxSession::_mfxSession(const mfxU32 adapterNum)
    : m_coreInt()
    , m_currentPlatform()
    , m_adapterNum(adapterNum)
    , m_implInterface()
    , m_pScheduler()
    , m_priority()
    , m_version()
    , m_pOperatorCore()
    , m_bIsHWENCSupport()
    , m_bIsHWDECSupport()
{
#if defined (MFX_VA)
    m_currentPlatform = MFX_PLATFORM_HARDWARE;
#else
    m_currentPlatform = MFX_PLATFORM_SOFTWARE;
#endif

    Clear();
} // _mfxSession::_mfxSession(const mfxU32 adapterNum) :

_mfxSession::~_mfxSession(void)
{
    Cleanup();

} // _mfxSession::~_mfxSession(void)

void _mfxSession::Clear(void)
{
    m_pScheduler = NULL;
    m_pSchedulerAllocated = NULL;

    m_priority = MFX_PRIORITY_NORMAL;
    m_bIsHWENCSupport = false;
    //m_coreInt.ExternalSurfaceAllocator = 0;

} // void _mfxSession::Clear(void)

void _mfxSession::Cleanup(void)
{
    // unregister plugin before closing
    if (m_plgGen.get())
    {
        m_plgGen->PluginClose();
    }
    if (m_plgEnc.get())
    {
        m_plgEnc->PluginClose();
    }
    if (m_plgDec.get())
    {
        m_plgDec->PluginClose();
    }
    if (m_plgVPP.get())
    {
        m_plgVPP->PluginClose();
    }

    if (m_pScheduler)
    {
        m_pScheduler->Release();
    }
    if (m_pSchedulerAllocated)
    {
        m_pSchedulerAllocated->Release();
    }

    // release the components the excplicit way.
    // do not relay on default deallocation order,
    // somebody could change it.
    m_plgGen.reset();
    m_pPAK.reset();
    m_pENC.reset();
    m_pVPP.reset();
    m_pDECODE.reset();
    m_pENCODE.reset();
    m_pCORE.reset();

    //delete m_coreInt.ExternalSurfaceAllocator;
    Clear();

} // void _mfxSession::Release(void)

mfxStatus _mfxSession::Init(mfxIMPL implInterface, mfxVersion *ver)
{
    mfxStatus mfxRes;
    MFX_SCHEDULER_PARAM schedParam;
    mfxU32 maxNumThreads;
#if defined(MFX_VA_WIN)
    bool isExternalThreading = (implInterface & MFX_IMPL_EXTERNAL_THREADING)?true:false;
    implInterface &= ~MFX_IMPL_EXTERNAL_THREADING;
#endif
    // release the object before initialization
    Cleanup();

    if (ver)
    {
        m_version = *ver;
    }
    else
    {
        mfxStatus sts = MFXQueryVersion(this, &m_version);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    // save working HW interface
    switch (implInterface&-MFX_IMPL_VIA_ANY)
    {
        // if nothing is set, nothing is returned
    case MFX_IMPL_UNSUPPORTED:
        m_implInterface = MFX_IMPL_UNSUPPORTED;
        break;
#if defined(MFX_VA_WIN)        
        // D3D9 is only one supported interface
    case MFX_IMPL_VIA_ANY:
#endif    
    case MFX_IMPL_VIA_D3D9:
        m_implInterface = MFX_IMPL_VIA_D3D9;
        break;
    case MFX_IMPL_VIA_D3D11:
        m_implInterface = MFX_IMPL_VIA_D3D11;
        break;
        
#if defined(MFX_VA_LINUX)        
        // VAAPI is only one supported interface
    case MFX_IMPL_VIA_ANY:
    case MFX_IMPL_VIA_VAAPI:
        m_implInterface = MFX_IMPL_VIA_VAAPI;
        break;
#endif        

        // unknown hardware interface
    default:
        if (MFX_PLATFORM_HARDWARE == m_currentPlatform)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    // get the number of available threads
    maxNumThreads = vm_sys_info_get_cpu_num();

    // allocate video core
    if (MFX_PLATFORM_SOFTWARE == m_currentPlatform)
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_NO, 0, maxNumThreads, this));
    }
#if defined(MFX_VA_WIN)
    else
    {
        if (MFX_IMPL_VIA_D3D11 == m_implInterface)
        {
#if defined (MFX_D3D11_ENABLED)
            m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_D3D11, m_adapterNum, maxNumThreads, this));
#else
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
#endif
        }
        else
            m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_D3D9, m_adapterNum, maxNumThreads, this));
        
    }
#elif defined(MFX_VA_LINUX)
    else
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_VAAPI, m_adapterNum, maxNumThreads, this));
    }
    
#elif defined(MFX_VA_OSX)
    
    else
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_VDAAPI, m_adapterNum, maxNumThreads, this));
    }
#endif

    // initialize the core interface
    InitCoreInterface(&m_coreInt, this);

    // query the scheduler interface
    m_pScheduler = QueryInterface<MFXIScheduler> (m_pSchedulerAllocated,
                                                  MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }
    memset(&schedParam, 0, sizeof(schedParam));
    schedParam.flags = MFX_SCHEDULER_DEFAULT;
#if defined(MFX_VA_WIN)
    if (isExternalThreading)
        schedParam.flags = MFX_SINGLE_THREAD;
#endif
    schedParam.numberOfThreads = maxNumThreads;
    schedParam.pCore = m_pCORE.get();
    mfxRes = m_pScheduler->Initialize(&schedParam);
    if (MFX_ERR_NONE != mfxRes)
    {
        return mfxRes;
    }

    m_pOperatorCore = new OperatorCORE(m_pCORE.get());

    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::Init(mfxIMPL implInterface)

mfxStatus _mfxSession::RestoreScheduler(void)
{
    if(m_pSchedulerAllocated)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    // leave the current scheduler
    if (m_pScheduler)
    {
        m_pScheduler->Release();
        m_pScheduler = NULL;
    }

    // query the scheduler interface
    m_pScheduler = QueryInterface<MFXIScheduler> (m_pSchedulerAllocated,
                                                  MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::RestoreScheduler(void)

mfxStatus _mfxSession::ReleaseScheduler(void)
{
    if(m_pScheduler)
        m_pScheduler->Release();
    
    if(m_pSchedulerAllocated)
    m_pSchedulerAllocated->Release();

    m_pScheduler = NULL;
    m_pSchedulerAllocated = NULL;
    
    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::RestoreScheduler(void)

//////////////////////////////////////////////////////////////////////////
// _mfxSession_1_10 own members
//////////////////////////////////////////////////////////////////////////

_mfxSession_1_10::_mfxSession_1_10(mfxU32 adapterNum)
    : _mfxSession(adapterNum)
    , m_refCounter(1)
    , m_externalThreads(0)
{
}

_mfxSession_1_10::~_mfxSession_1_10(void)
{
    if (m_plgPreEnc.get())
    {
        m_plgPreEnc->PluginClose();
    }
    m_plgPreEnc.reset();
}

//////////////////////////////////////////////////////////////////////////
// _mfxSession_1_10::MFXISession_1_10 members
//////////////////////////////////////////////////////////////////////////


void _mfxSession_1_10::SetAdapterNum(const mfxU32 adapterNum)
{
    m_adapterNum = adapterNum;
}

std::unique_ptr<VideoCodecUSER> &  _mfxSession_1_10::GetPreEncPlugin()
{
    return m_plgPreEnc;
}

//////////////////////////////////////////////////////////////////////////
// _mfxSession_1_10::MFXIUnknown members
//////////////////////////////////////////////////////////////////////////

void *_mfxSession_1_10::QueryInterface(const MFX_GUID &guid)
{
    // Specific interface is required
    if (MFXISession_1_10_GUID == guid)
    {
        // increment reference counter
        vm_interlocked_inc32(&m_refCounter);

        return (MFXISession_1_10 *) this;
    }

    // it is unsupported interface
    return NULL;

} // void *_mfxSession_1_10::QueryInterface(const MFX_GUID &guid)

void _mfxSession_1_10::AddRef(void)
{
    // increment reference counter
    vm_interlocked_inc32(&m_refCounter);

} // void mfxSchedulerCore::AddRef(void)

void _mfxSession_1_10::Release(void)
{
    // decrement reference counter
    vm_interlocked_dec32(&m_refCounter);

    if (0 == m_refCounter)
    {
        delete this;
    }

} // void _mfxSession_1_10::Release(void)

mfxU32 _mfxSession_1_10::GetNumRef(void) const
{
    return m_refCounter;

} // mfxU32 _mfxSession_1_10::GetNumRef(void) const

mfxStatus _mfxSession_1_10::InitEx(mfxInitParam& par)
{
    mfxStatus mfxRes;
    mfxU32 maxNumThreads;
#if defined(MFX_VA_WIN)
    bool isSingleThreadMode = (par.Implementation & MFX_IMPL_EXTERNAL_THREADING) ? true : false;
    par.Implementation &= ~MFX_IMPL_EXTERNAL_THREADING;
#endif
    // release the object before initialization
    Cleanup();

    m_version = par.Version;

    // save working HW interface
    switch (par.Implementation&-MFX_IMPL_VIA_ANY)
    {
        // if nothing is set, nothing is returned
    case MFX_IMPL_UNSUPPORTED:
        m_implInterface = MFX_IMPL_UNSUPPORTED;
        break;
#if defined(MFX_VA_WIN)        
        // D3D9 is only one supported interface
    case MFX_IMPL_VIA_ANY:
#endif    
    case MFX_IMPL_VIA_D3D9:
        m_implInterface = MFX_IMPL_VIA_D3D9;
        break;
    case MFX_IMPL_VIA_D3D11:
        m_implInterface = MFX_IMPL_VIA_D3D11;
        break;

#if defined(MFX_VA_LINUX)        
        // VAAPI is only one supported interface
    case MFX_IMPL_VIA_ANY:
    case MFX_IMPL_VIA_VAAPI:
        m_implInterface = MFX_IMPL_VIA_VAAPI;
        break;
#endif        

        // unknown hardware interface
    default:
        if (MFX_PLATFORM_HARDWARE == m_currentPlatform)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    // only mfxExtThreadsParam is allowed
    if (par.NumExtParam)
    {
        if ((par.NumExtParam > 1) || !par.ExtParam)
        {
            return MFX_ERR_UNSUPPORTED;
        }
        if ((par.ExtParam[0]->BufferId != MFX_EXTBUFF_THREADS_PARAM) ||
            (par.ExtParam[0]->BufferSz != sizeof(mfxExtThreadsParam)))
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }
#if defined(_WIN32) || defined(_WIN64)
    if (par.NumExtParam || par.ExtParam)
    {
        return MFX_ERR_UNSUPPORTED;
    }
#endif

    // get the number of available threads
    maxNumThreads = (par.ExternalThreads != 0) ? 0 : vm_sys_info_get_cpu_num();

    // allocate video core
    if (MFX_PLATFORM_SOFTWARE == m_currentPlatform)
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_NO, 0, maxNumThreads, this));
    }
#if defined(MFX_VA_WIN)
    else
    {
        if (MFX_IMPL_VIA_D3D11 == m_implInterface)
        {
#if defined (MFX_D3D11_ENABLED)
            m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_D3D11, m_adapterNum, maxNumThreads, this));
#else
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
#endif
        }
        else
        {
            D3D9DllCallHelper d3d9hlp;
            if (d3d9hlp.isD3D9Available() == false)
                return  MFX_ERR_UNSUPPORTED;

            m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_D3D9, m_adapterNum, maxNumThreads, this));
        }

    }
#elif defined(MFX_VA_LINUX)
    else
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_VAAPI, m_adapterNum, maxNumThreads, this));
    }

#elif defined(MFX_VA_OSX)

    else
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_VDAAPI, m_adapterNum, maxNumThreads, this));
    }
#endif

    // initialize the core interface
    InitCoreInterface(&m_coreInt, this);

    // query the scheduler interface
    m_pScheduler = ::QueryInterface<MFXIScheduler>(m_pSchedulerAllocated, MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }

    MFXIScheduler2* pScheduler2 = ::QueryInterface<MFXIScheduler2>(m_pSchedulerAllocated, MFXIScheduler2_GUID);

    if (par.NumExtParam && !pScheduler2) {
        return MFX_ERR_UNKNOWN;
    }

    if (pScheduler2) {
        MFX_SCHEDULER_PARAM2 schedParam;
        memset(&schedParam, 0, sizeof(schedParam));
        schedParam.flags = MFX_SCHEDULER_DEFAULT;
#if defined(MFX_VA_WIN)
        if (isSingleThreadMode)
            schedParam.flags = MFX_SINGLE_THREAD;
#endif
        schedParam.numberOfThreads = maxNumThreads;
        schedParam.pCore = m_pCORE.get();
        if (par.NumExtParam) {
            schedParam.params = *((mfxExtThreadsParam*)par.ExtParam[0]);
        }
        mfxRes = pScheduler2->Initialize2(&schedParam);

        m_pScheduler->Release();
    }
    else {
        MFX_SCHEDULER_PARAM schedParam;
        memset(&schedParam, 0, sizeof(schedParam));
        schedParam.flags = MFX_SCHEDULER_DEFAULT;
#if defined(MFX_VA_WIN)
        if (isSingleThreadMode)
            schedParam.flags = MFX_SINGLE_THREAD;
#endif
        schedParam.numberOfThreads = maxNumThreads;
        schedParam.pCore = m_pCORE.get();
        mfxRes = m_pScheduler->Initialize(&schedParam);
    }

    if (MFX_ERR_NONE != mfxRes) {
        return mfxRes;
    }

    m_pOperatorCore = new OperatorCORE(m_pCORE.get());

    if (MFX_PLATFORM_SOFTWARE == m_currentPlatform && MFX_GPUCOPY_ON == par.GPUCopy)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    // Linux: By default CM Copy disabled on HW cores so only need to handle explicit ON value
    // Windows: By default CM Copy enabled on HW cores, so only need to handle explicit OFF value
    const bool disableGpuCopy = (m_pCORE->GetVAType() == MFX_HW_VAAPI)
        ? (MFX_GPUCOPY_ON != par.GPUCopy)
        : (MFX_GPUCOPY_OFF == par.GPUCopy);

    if (disableGpuCopy)
    {
        CMEnabledCoreInterface* pCmCore = QueryCoreInterface<CMEnabledCoreInterface>(m_pCORE.get());
        if (pCmCore)
        {
            mfxRes = pCmCore->SetCmCopyStatus(false);
        }
        if (MFX_ERR_NONE != mfxRes) {
            return mfxRes;
        }
    }

    return MFX_ERR_NONE;
} // mfxStatus _mfxSession_1_10::InitEx(mfxInitParam& par);


//explicit specification of interface creation
template<> MFXISession_1_10*  CreateInterfaceInstance<MFXISession_1_10>(const MFX_GUID &guid)
{
    if (MFXISession_1_10_GUID == guid)
        return (MFXISession_1_10*) (new _mfxSession_1_10(0));

    return NULL;
}

namespace MFX
{
    unsigned int CreateUniqId()
    {
        static volatile mfxU32 g_tasksId = 0;
        return (unsigned int)vm_interlocked_inc32(&g_tasksId);
    }
}
