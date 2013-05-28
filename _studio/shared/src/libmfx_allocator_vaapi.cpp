/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2013 Intel Corporation. All Rights Reserved.

File Name: libmfx_allocator_vaapi.cpp

\* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_VA_LINUX)

#include <algorithm>
#include <vector>

#include "ippcore.h"
#include "ipps.h"

#include "libmfx_allocator_vaapi.h"
#include "mfx_utils.h"

#define VA_SAFE_CALL(__CALL)        \
{                                   \
    VAStatus va_sts = __CALL;       \
    if (VA_STATUS_SUCCESS != va_sts) return MFX_ERR_INVALID_HANDLE; \
}

#define VA_TO_MFX_STATUS(_va_res) \
    (VA_STATUS_SUCCESS == (_va_res))? MFX_ERR_NONE: MFX_ERR_DEVICE_FAILED;

unsigned int ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return VA_FOURCC_NV12;
    case MFX_FOURCC_YV12:
        return VA_FOURCC_YV12;
    case MFX_FOURCC_YUY2:
        return VA_FOURCC_YUY2;
    case MFX_FOURCC_RGB4:
        return VA_FOURCC_ARGB;
    case MFX_FOURCC_P8:
        return VA_FOURCC_P208;

    default:
        VM_ASSERT(!"unsupported fourcc");
        return 0;
    }
}

mfxStatus
mfxDefaultAllocatorVAAPI::AllocFramesHW(
    mfxHDL                  pthis,
    mfxFrameAllocRequest*   request,
    mfxFrameAllocResponse*  response)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus va_sts = VA_STATUS_SUCCESS;
    unsigned int width = request->Info.Width;
    unsigned int height = request->Info.Height;
    mfxU32 fourcc = request->Info.FourCC;
    mfxU16 maxNumFrames = request->NumFrameSuggested; //(mfxU16)IPP_MIN(MFX_NUM_FRAME_HANDLES, request->NumFrameSuggested);
    unsigned int va_fourcc = 0;
    VASurfaceAttrib attrib;
    VASurfaceID* surfaces = NULL;
    vaapiMemIdInt *vaapi_mids = NULL, *vaapi_mid = NULL;
    bool bCreateSrfSucceeded = false;
    mfxU16 numAllocated = 0, i = 0;

    if (!pthis)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
    va_fourcc = ConvertMfxFourccToVAFormat(fourcc);
    if (!va_fourcc || ((VA_FOURCC_NV12 != va_fourcc) &&
                       (VA_FOURCC_YV12 != va_fourcc) &&
                       (VA_FOURCC_YUY2 != va_fourcc) &&
                       (VA_FOURCC_ARGB != va_fourcc) &&
                       (VA_FOURCC_P208 != va_fourcc)))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    if (!maxNumFrames)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

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

    // allocate frames in cycle
    if (MFX_ERR_NONE == mfx_res)
    {
        surfaces = (VASurfaceID*)calloc(maxNumFrames, sizeof(VASurfaceID));
        vaapi_mids = (vaapiMemIdInt*)calloc(maxNumFrames, sizeof(vaapiMemIdInt));
        if ((NULL == surfaces) || (NULL == vaapi_mids))
        {
            mfx_res = MFX_ERR_MEMORY_ALLOC;
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        if( MFX_FOURCC_P8 != fourcc )
        {
            attrib.type = VASurfaceAttribPixelFormat;
            attrib.value.type = VAGenericValueTypeInteger;
            attrib.value.value.i = va_fourcc;
            attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;

            va_sts = vaCreateSurfaces(pSelf->pVADisplay, VA_RT_FORMAT_YUV420, width, height, surfaces, maxNumFrames, &attrib, 1);
            mfx_res = VA_TO_MFX_STATUS(va_sts);

            bCreateSrfSucceeded = (MFX_ERR_NONE == mfx_res);
        }
        else
        {
            VAContextID context_id = request->reserved[0];
            int codedbuf_size = (width * height) * 400 / (16 * 16); //from libva spec

            for (numAllocated = 0; numAllocated < maxNumFrames; numAllocated++)
            {
                VABufferID coded_buf;

                va_sts = vaCreateBuffer(pSelf->pVADisplay,
                                    context_id,
                                    VAEncCodedBufferType,
                                    codedbuf_size,
                                    1,
                                    NULL,
                                    &coded_buf);
                mfx_res = VA_TO_MFX_STATUS(va_sts);
                if (MFX_ERR_NONE != mfx_res) break;
                surfaces[numAllocated] = coded_buf;
            }
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        for (i = 0; i < maxNumFrames; ++i)
        {
            vaapi_mid = &(vaapi_mids[i]);
            vaapi_mid->m_fourcc = fourcc;
            vaapi_mid->m_surface = &(surfaces[i]);

            pSelf->m_frameHandles.push_back(vaapi_mid);
        }
        response->mids = &pSelf->m_frameHandles[0];
        response->NumFrameActual = maxNumFrames;
        pSelf->NumFrames = maxNumFrames;
    }
    else
    {
        response->mids = NULL;
        response->NumFrameActual = 0;
        if (MFX_FOURCC_P8 != fourcc)
        {
            if (bCreateSrfSucceeded) vaDestroySurfaces(pSelf->pVADisplay, surfaces, maxNumFrames);
        }
        else
        {
            for (i = 0; i < numAllocated; i++)
                vaDestroyBuffer(pSelf->pVADisplay, surfaces[i]);
        }
        if (vaapi_mids) { free(vaapi_mids); vaapi_mids = NULL; }
        if (surfaces) { free(surfaces); surfaces = NULL; }
    }
    return mfx_res;
}

mfxStatus
mfxDefaultAllocatorVAAPI::LockFrameHW(
    mfxHDL         pthis,
    mfxMemId       mid,
    mfxFrameData*  ptr)
{
    MFX_CHECK(pthis, MFX_ERR_INVALID_HANDLE);
    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;

    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_sts  = VA_STATUS_SUCCESS;
    vaapiMemIdInt* vaapi_mids = (vaapiMemIdInt*)mid;
    mfxU8* pBuffer = 0;

    if (!vaapi_mids || !(vaapi_mids->m_surface)) return MFX_ERR_INVALID_HANDLE;

    if (MFX_FOURCC_P8 == vaapi_mids->m_fourcc)   // bitstream processing
    {
        VACodedBufferSegment *coded_buffer_segment;
        va_sts =  vaMapBuffer(pSelf->pVADisplay, *(vaapi_mids->m_surface), (void **)(&coded_buffer_segment));
        mfx_res = VA_TO_MFX_STATUS(va_sts);
        if (MFX_ERR_NONE == mfx_res)
            ptr->Y = (mfxU8*)coded_buffer_segment->buf;
    }
    else
    {
        va_sts = vaSyncSurface(pSelf->pVADisplay, *(vaapi_mids->m_surface));
        mfx_res = VA_TO_MFX_STATUS(va_sts);
#if 0
        if (MFX_ERR_NONE == mfx_res)
        {
            VASurfaceStatus srf_sts = VASurfaceReady;

            va_sts = vaQuerySurfaceStatus(pSelf->pVADisplay, *(vaapi_mids->m_surface), &srf_sts);
            mfx_res = VA_TO_MFX_STATUS(va_sts);
        }
#endif
        if (MFX_ERR_NONE == mfx_res)
        {
            va_sts = vaDeriveImage(pSelf->pVADisplay, *(vaapi_mids->m_surface), &(vaapi_mids->m_image));
            mfx_res = VA_TO_MFX_STATUS(va_sts);
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            va_sts = vaMapBuffer(pSelf->pVADisplay, vaapi_mids->m_image.buf, (void **) &pBuffer);
            mfx_res = VA_TO_MFX_STATUS(va_sts);
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            switch (vaapi_mids->m_image.format.fourcc)
            {
            case VA_FOURCC_NV12:
                if (vaapi_mids->m_fourcc == MFX_FOURCC_NV12)
                {
                    ptr->Pitch = (mfxU16)vaapi_mids->m_image.pitches[0];
                    ptr->Y = pBuffer + vaapi_mids->m_image.offsets[0];
                    ptr->U = pBuffer + vaapi_mids->m_image.offsets[1];
                    ptr->V = ptr->U + 1;
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_YV12:
                if (vaapi_mids->m_fourcc == MFX_FOURCC_YV12)
                {
                    ptr->Pitch = (mfxU16)vaapi_mids->m_image.pitches[0];
                    ptr->Y = pBuffer + vaapi_mids->m_image.offsets[0];
                    ptr->V = pBuffer + vaapi_mids->m_image.offsets[1];
                    ptr->U = pBuffer + vaapi_mids->m_image.offsets[2];
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_YUY2:
                if (vaapi_mids->m_fourcc == MFX_FOURCC_YUY2)
                {
                    ptr->Pitch = (mfxU16)vaapi_mids->m_image.pitches[0];
                    ptr->Y = pBuffer + vaapi_mids->m_image.offsets[0];
                    ptr->U = ptr->Y + 1;
                    ptr->V = ptr->Y + 3;
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_ARGB:
                if (vaapi_mids->m_fourcc == MFX_FOURCC_RGB4)
                {
                    ptr->Pitch = (mfxU16)vaapi_mids->m_image.pitches[0];
                    ptr->B = pBuffer + vaapi_mids->m_image.offsets[0];
                    ptr->G = ptr->B + 1;
                    ptr->R = ptr->B + 2;
                    ptr->A = ptr->B + 3;
                }
                else mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            default:
                mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            }
        }
    }
    return mfx_res;
}

mfxStatus
mfxDefaultAllocatorVAAPI::GetHDLHW(
    mfxHDL    pthis,
    mfxMemId  mid,
    mfxHDL*   handle)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    //mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    if (0 == mid)
        return MFX_ERR_INVALID_HANDLE;

    vaapiMemIdInt* vaapi_mids = (vaapiMemIdInt*)mid;

    if (!handle || !vaapi_mids || !(vaapi_mids->m_surface)) return MFX_ERR_INVALID_HANDLE;

    *handle = vaapi_mids->m_surface; //VASurfaceID* <-> mfxHDL
    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorVAAPI::UnlockFrameHW(
    mfxHDL         pthis,
    mfxMemId       mid,
    mfxFrameData*  ptr)
{
    // TBD
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;

    vaapiMemIdInt* vaapi_mids = (vaapiMemIdInt*)mid;

    if (!vaapi_mids || !(vaapi_mids->m_surface)) return MFX_ERR_INVALID_HANDLE;

    if (MFX_FOURCC_P8 == vaapi_mids->m_fourcc)   // bitstream processing
    {
        vaUnmapBuffer(pSelf->pVADisplay, *(vaapi_mids->m_surface));
    }
    else  // Image processing
    {
        vaUnmapBuffer(pSelf->pVADisplay, vaapi_mids->m_image.buf);
        vaDestroyImage(pSelf->pVADisplay, vaapi_mids->m_image.image_id);

        if (NULL != ptr)
        {
            ptr->Pitch = 0;
            ptr->Y     = NULL;
            ptr->U     = NULL;
            ptr->V     = NULL;
            ptr->A     = NULL;
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorVAAPI::FreeFramesHW(
    mfxHDL                  pthis,
    mfxFrameAllocResponse*  response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;

    vaapiMemIdInt *vaapi_mids = NULL;
    VASurfaceID* surfaces = NULL;
    mfxU32 i = 0;
    bool isBitstreamMemory = false;

    if (!response) return MFX_ERR_NULL_PTR;

    if (response->mids)
    {
        vaapi_mids = (vaapiMemIdInt*)(response->mids[0]);
        isBitstreamMemory = (MFX_FOURCC_P8 == vaapi_mids->m_fourcc)?true:false;
        surfaces = vaapi_mids->m_surface;
        for (i = 0; i < response->NumFrameActual; ++i)
        {
            if (MFX_FOURCC_P8 == vaapi_mids[i].m_fourcc)
            {
                vaDestroyBuffer(pSelf->pVADisplay, surfaces[i]);
            }
        }
        free(vaapi_mids);
        response->mids = NULL;

        if (!isBitstreamMemory) vaDestroySurfaces(pSelf->pVADisplay, surfaces, response->NumFrameActual);
        free(surfaces);
    }
    response->NumFrameActual = 0;

    return MFX_ERR_NONE;
}

mfxDefaultAllocatorVAAPI::mfxWideHWFrameAllocator::mfxWideHWFrameAllocator(
    mfxU16  type,
    mfxHDL  handle)
    :
    mfxBaseWideFrameAllocator(type)
{
    frameAllocator.Alloc = &mfxDefaultAllocatorVAAPI::AllocFramesHW;
    frameAllocator.Lock = &mfxDefaultAllocatorVAAPI::LockFrameHW;
    frameAllocator.GetHDL = &mfxDefaultAllocatorVAAPI::GetHDLHW;
    frameAllocator.Unlock = &mfxDefaultAllocatorVAAPI::UnlockFrameHW;
    frameAllocator.Free = &mfxDefaultAllocatorVAAPI::FreeFramesHW;
    pVADisplay = (VADisplay *)handle;
}
#endif // (MFX_VA_LINUX)
/* EOF */
