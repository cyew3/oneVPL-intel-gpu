//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_VA_OSX)

#include <algorithm>
#include <vector>

#include "ippcore.h"
#include "ipps.h"

#include "libmfx_allocator_vdaapi.h"
#include "mfx_utils.h"

#define AET_DEBUG

OSType ConvertMfxFourccToPixelFormatType(mfxU32 fourcc)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
            return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
/*        case MFX_FOURCC_YUY2:
            return VA_FOURCC_YUY2;
        case MFX_FOURCC_YV12:
            return VA_FOURCC_YV12;
        case MFX_FOURCC_RGB4:
            return VA_FOURCC_ARGB;
        case MFX_FOURCC_P8:
            return VA_FOURCC_P208;*/
            
        default:
            assert(!"unsupported fourcc");
            return 0;
    }
}

mfxStatus
mfxDefaultAllocatorVDAAPI::AllocFramesHW(
    mfxHDL                  pthis,
    mfxFrameAllocRequest*   request,
    mfxFrameAllocResponse*  response)
{
#ifdef AET_DEBUG
    printf("mfxDefaultAllocatorVDAAPI::%s entry\n", __FUNCTION__);
#endif
    
    mfxStatus mfx_res = MFX_ERR_NONE;
    unsigned int width = request->Info.Width;
    unsigned int height = request->Info.Height;
    
    OSType vtbPixelFormatType = 0;
    const unsigned int minimumFrameCount = 15;

    mfxU32 fourcc = request->Info.FourCC;
    mfxU16 maxNumFrames = request->NumFrameSuggested;
    vtbapiMemIdInt * vtbapi_mids = NULL, * vtbapi_mid = NULL;
    mfxMemId * mids = NULL;

    bool bCreateSrfSucceeded = false;
    mfxU16 numAllocated = 0, i = 0;

    if (!pthis)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
    vtbPixelFormatType = ConvertMfxFourccToPixelFormatType(fourcc);
    if (!vtbPixelFormatType || ((kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange != vtbPixelFormatType) && (kCVPixelFormatType_422YpCbCr8 != vtbPixelFormatType)))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    if (!maxNumFrames)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    mfxWideHWFrameAllocator * pSelf = (mfxWideHWFrameAllocator *) pthis;

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

    // allocate frames in cycle
    if (MFX_ERR_NONE == mfx_res)
    {
        //Allocate and clear memory for enough vtbapi_mids for all surfaces
        vtbapi_mids = (vtbapiMemIdInt *) calloc(maxNumFrames, sizeof(vtbapiMemIdInt));
        mids = (mfxMemId *) calloc(maxNumFrames, sizeof(mfxMemId));
        if ((NULL == vtbapi_mids) || (NULL == mids))
        {
            mfx_res = MFX_ERR_MEMORY_ALLOC;
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        for (i = 0; i < maxNumFrames; ++i)
        {
            vtbapi_mid = &(vtbapi_mids[i]);
            vtbapi_mid->m_fourcc = fourcc;
            vtbapi_mid->m_pixelFormatType = vtbPixelFormatType;
            vtbapi_mid->m_imageBuffer = NULL;
            vtbapi_mid->m_backingIOSurface = NULL;
            vtbapi_mid->isLocked = false;
            pSelf->m_frameHandles.push_back(vtbapi_mid);
            mids[i] = vtbapi_mid;
        }
        response->mids = &pSelf->m_frameHandles[0];
        response->NumFrameActual = maxNumFrames;
        pSelf->NumFrames = maxNumFrames;
    }
    else
    {
        response->mids = NULL;
        response->NumFrameActual = 0;
        
        if (mids)
        {
            free(mids);
            mids = NULL;
        }
        if (vtbapi_mids)
        {
            free(vtbapi_mids);
            vtbapi_mids = NULL;
        }
    }
    return mfx_res;
}

mfxStatus
mfxDefaultAllocatorVDAAPI::LockFrameHW(
    mfxHDL         pthis,
    mfxMemId       mid,
    mfxFrameData*  ptr)
{
#ifdef AET_DEBUG
    printf("mfxDefaultAllocatorVDAAPI::%s entry\n", __FUNCTION__);
#endif
    
    MFX_CHECK(pthis, MFX_ERR_INVALID_HANDLE);
    mfxWideHWFrameAllocator * pSelf = (mfxWideHWFrameAllocator *) pthis;

    mfxStatus mfx_res = MFX_ERR_NONE;
    vtbapiMemIdInt * vtbapi_mid = (vtbapiMemIdInt *) mid;
    mfxU8* pBuffer = 0;

    if (!vtbapi_mid || !(vtbapi_mid->m_imageBuffer))
    {
        return MFX_ERR_INVALID_HANDLE;
    }

    IOSurfaceRef backingIOSurface = vtbapi_mid->m_backingIOSurface;
    
    if(!backingIOSurface)
    {
#ifdef AET_DEBUG
        printf(" mfxDefaultAllocatorVDAAPI::%s early exit, backingIOSurface is NULL\n", __FUNCTION__);
#endif
        return MFX_ERR_LOCK_MEMORY;
    }
    
#ifdef AET_DEBUG
    printf(" mfxDefaultAllocatorVDAAPI::%s backingIOSurface = %p\n", __FUNCTION__, backingIOSurface);
#endif
    
    mfxU8 * pBaseAddressY = NULL;
    mfxU8 * pBaseAddressU = NULL;
    size_t planeCount;
    size_t bytesPerRowY;
    size_t bytesPerRowU;
    
    if (MFX_FOURCC_NV12 == vtbapi_mid->m_fourcc)   // bitstream processing
    {
        
        OSType pixelFormatType = IOSurfaceGetPixelFormat(backingIOSurface);
        if(pixelFormatType != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
        {
            //expected NV12, got something else
            mfx_res = MFX_ERR_LOCK_MEMORY;
#ifdef AET_DEBUG
            printf("  mfxDefaultAllocatorVDAAPI::%s pixelFormatType != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange. mfx_res = MFX_ERR_LOCK_MEMORY\n", __FUNCTION__);
#endif
        }
        else
        {
            
#ifdef AET_DEBUG
            printf("  mfxDefaultAllocatorVDAAPI::%s LOCKING backingIOSurface at %p\n", __FUNCTION__, backingIOSurface);
#endif
            IOSurfaceLock(backingIOSurface, 0, NULL);   //No options, don't get internal modification seed value
            vtbapi_mid->isLocked = true;
            
            planeCount = IOSurfaceGetPlaneCount(backingIOSurface);
            pBaseAddressY = (mfxU8 *) IOSurfaceGetBaseAddressOfPlane(backingIOSurface, 0);
            bytesPerRowY = IOSurfaceGetBytesPerRowOfPlane(backingIOSurface, 0);
            if (planeCount > 0) {
                pBaseAddressU = (mfxU8 *) IOSurfaceGetBaseAddressOfPlane(backingIOSurface, 1);
                bytesPerRowU = IOSurfaceGetBytesPerRowOfPlane(backingIOSurface, 1);
            }
            
            if(bytesPerRowY != bytesPerRowU)
            {
                assert(!"mfxDefaultAllocatorVDAAPI::LockFrame planes have different pitches");
            }
            
#ifdef AET_DEBUG
            printf("  mfxDefaultAllocatorVDAAPI::%s Y bytes per row = %d, UV bytes per row = %d, plane count = %d\n", __FUNCTION__, bytesPerRowY, bytesPerRowU, planeCount);
#endif
            
            ptr->Pitch = (mfxU16) bytesPerRowY;
            ptr->Y = pBaseAddressY;
            ptr->U = pBaseAddressU;
            ptr->V = ptr->U + 1;
        }
    }
    else
    {
#ifdef AET_DEBUG
        printf("  mfxDefaultAllocatorVDAAPI::%s MFX_FOURCC_NV12 != vtbapi_mid->m_fourcc. mfx_res = MFX_ERR_LOCK_MEMORY\n", __FUNCTION__);
#endif        }
        mfx_res = MFX_ERR_LOCK_MEMORY;
    }
    return mfx_res;
}

mfxStatus
mfxDefaultAllocatorVDAAPI::GetHDLHW(
    mfxHDL    pthis,
    mfxMemId  mid,
    mfxHDL*   handle)
{
#ifdef AET_DEBUG
    printf("mfxDefaultAllocatorVDAAPI::%s entry\n", __FUNCTION__);
#endif
    
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    if (0 == mid)
        return MFX_ERR_INVALID_HANDLE;

    struct vtbapiMemIdInt * vdaapi_mids = (struct vtbapiMemIdInt *) mid;

    if (!handle || !vdaapi_mids || !(vdaapi_mids->m_backingIOSurface)) return MFX_ERR_INVALID_HANDLE;

    *handle = vdaapi_mids->m_imageBuffer; //m_imageBuffer <-> mfxHDL
    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorVDAAPI::UnlockFrameHW(
    mfxHDL         pthis,
    mfxMemId       mid,
    mfxFrameData*  ptr)
{
    
#ifdef AET_DEBUG
    printf("mfxDefaultAllocatorVDAAPI::%s entry\n", __FUNCTION__);
#endif
    
    // TBD
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator * pSelf = (mfxWideHWFrameAllocator *) pthis;

    struct vtbapiMemIdInt* vdaapi_mids = (struct vtbapiMemIdInt *) mid;

    if (!vdaapi_mids || !(vdaapi_mids->m_backingIOSurface))
    {
        return MFX_ERR_INVALID_HANDLE;
    }

    if(vdaapi_mids->isLocked)
    {
#ifdef AET_DEBUG
        printf(" mfxDefaultAllocatorVDAAPI::%s UNLOCKING imageBuffer at %p\n", __FUNCTION__, vdaapi_mids->m_imageBuffer);
#endif
#ifndef EXPERIMENT_MINIMAL_PROCESSING
        CVPixelBufferUnlockBaseAddress(vdaapi_mids->m_imageBuffer, 0);
#endif
        vdaapi_mids->isLocked = false;
    }
    //    CVPixelBufferRelease(vtbapi_mid->m_imageBuffer);
    vdaapi_mids->m_imageBuffer = NULL;
    vdaapi_mids->m_backingIOSurface = NULL;

    if (NULL != ptr)
    {
        ptr->Pitch = 0;
        ptr->Y     = NULL;
        ptr->U     = NULL;
        ptr->V     = NULL;
        ptr->A     = NULL;
    }
    
    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorVDAAPI::FreeFramesHW(
    mfxHDL                  pthis,
    mfxFrameAllocResponse*  response)
{
#ifdef AET_DEBUG
    printf("mfxDefaultAllocatorVDAAPI::%s entry\n", __FUNCTION__);
#endif
    
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    
    struct vtbapiMemIdInt * vtbapi_mids = NULL;
    mfxU32 i = 0;
    
    if (!response) return MFX_ERR_NULL_PTR;
    
    if (response->mids)
    {
        vtbapi_mids = (struct vtbapiMemIdInt *) (response->mids[0]);
        for (i = 0; i < response->NumFrameActual; ++i)
        {
            if (vtbapi_mids[i].m_imageBuffer)
            {
                CFIndex retainCount;
                if(retainCount = CFGetRetainCount(vtbapi_mids[i].m_imageBuffer)) {
                    if(vtbapi_mids[i].isLocked == true)
                    {
                        CVPixelBufferUnlockBaseAddress(vtbapi_mids[i].m_imageBuffer, 0);
                        vtbapi_mids[i].isLocked = false;
                    }
                    CVPixelBufferRelease(vtbapi_mids[i].m_imageBuffer);
                    vtbapi_mids[i].m_imageBuffer = NULL;
                    vtbapi_mids[i].m_backingIOSurface = NULL;
                }
            }
        }
        free(vtbapi_mids);
        free(response->mids);
        response->mids = NULL;
    }
    
    response->NumFrameActual = 0;
    return MFX_ERR_NONE;
}

mfxDefaultAllocatorVDAAPI::mfxWideHWFrameAllocator::mfxWideHWFrameAllocator(
    mfxU16  type,
    mfxHDL  handle)
    :
    mfxBaseWideFrameAllocator(type)
{
    frameAllocator.Alloc = &mfxDefaultAllocatorVDAAPI::AllocFramesHW;
    frameAllocator.Lock = &mfxDefaultAllocatorVDAAPI::LockFrameHW;
    frameAllocator.GetHDL = &mfxDefaultAllocatorVDAAPI::GetHDLHW;
    frameAllocator.Unlock = &mfxDefaultAllocatorVDAAPI::UnlockFrameHW;
    frameAllocator.Free = &mfxDefaultAllocatorVDAAPI::FreeFramesHW;
}
#endif // (MFX_VA_OSX)
/* EOF */
