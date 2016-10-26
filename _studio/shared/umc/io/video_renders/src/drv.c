//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2010 Intel Corporation. All Rights Reserved.
//

#include "drv.h"
#include <stdio.h>
#include <ipps.h>

#define MODULE "VDrv(abstract video driver)"
#define FUNCTION FUNCTION
#define ERR_STRING(msg)     VIDEO_DRV_ERR_STRING(MODULE, FUNCTION, msg)
#define ERR_SET(err, msg)   VIDEO_DRV_ERR_SET   (MODULE, err, FUNCTION, msg)
#define WRN_SET(err, msg)   VIDEO_DRV_WRN_SET   (MODULE, err, FUNCTION, msg)
#define DBG_SET(msg)        VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

size_t umc_vdrv_AlignValue(size_t nValue, size_t lAlign)
{
    return (nValue + (lAlign - 1)) & ~(lAlign - 1);
}

void* umc_vdrv_AlignPointer(void* pValue, size_t lAlign)
{
    return (void*)((((size_t)(pValue)) + (lAlign - 1)) & ~(lAlign - 1));
}

/* vm_status */
VIDEO_DRV_CREATE_BUFFERS_FUNC(umc_vdrv_CreateBuffers,
                              driver, min_b, max_b, bufs,
                              video_mem_type, video_mem_info)
{
#undef  FUNCTION
#define FUNCTION "umc_vdrv_CreateBuffers"
    vm_status               result  = VM_OK;
    VideoDrvVideoMemInfo*   drv_vm  = &(driver->m_VideoMemInfo);
    Ipp32u                  i, j;

    DBG_SET("+");
    if (VM_OK == result)
    {
        if ((NULL == driver) || (NULL == bufs))
        {
            ERR_SET(VM_NULL_PTR, "null ptr");
        }
    }
    if (VM_OK == result)
    {
        if ((min_b > max_b) || (0 == max_b))
        {
            ERR_SET(VM_OPERATION_FAILED, "number of buffers");
        }
    }
    if (VM_OK == result)
    {
        if (!(driver->m_DrvSpec.drv_videomem_type & video_mem_type))
        {
            ERR_SET(VM_OPERATION_FAILED, "specified video memory type is unsupported");
        }
    }
    if (VM_OK == result)
    {
        /* Copying video memory information. */
        if (NULL != video_mem_info)
        {
            ippsCopy_8u((const Ipp8u*)video_mem_info, (Ipp8u*)drv_vm,
                        sizeof(VideoDrvVideoMemInfo));
        }
        switch (video_mem_type)
        {
        case VideoDrvVideoMemLibrary:
            driver->m_VideoMemType  = VideoDrvVideoMemLibrary;
            /* Remark:
             *  Only next fields may be important:
             *  - driver->m_VideoMemInfo.m_nPitch;
             *  - driver->m_VideoMemInfo.m_nPitchAlignment;
             *  - driver->m_VideoMemInfo.m_nBufSize.
             *  Others fields will be ignored.
             */
            /* No video memory specified. Trying create buffers one by one. */
            for (i = 0; i < max_b; ++i)
            {
                bufs[i] = driver->m_DrvSpec.drvCreateSurface(driver);
                if (NULL == bufs[i]) break;
            }
            if (i < min_b)
            {
                ERR_SET(VM_OPERATION_FAILED, "umc_x_CreateSurface");
            }
            break;
        case VideoDrvVideoMemExternal:
            driver->m_VideoMemType  = VideoDrvVideoMemExternal;
            /* Remark:
             *  All fields of driver->m_VideoMemInfo.m_pVideoMem are important.
             */
            if ((NULL == drv_vm->m_pVideoMem) || (0 == drv_vm->m_nMemSize))
            {
                ERR_SET(VM_NULL_PTR, "null ptr for external video memory");
            }
            break;
        case VideoDrvVideoMemInternal:
            driver->m_VideoMemType  = VideoDrvVideoMemInternal;
            /* Remark:
             *  Fields:
             *  - driver->m_VideoMemInfo.m_pVideoMem;
             *  - driver->m_VideoMemInfo.m_nMemSize;
             *  will be ignored.
             */
            for (i = max_b; (i > 0) && (i >= min_b); --i)
            {
                drv_vm->m_nMemSize = i * drv_vm->m_nBufSize + drv_vm->m_nAlignment;
                drv_vm->m_pVideoMem = ippsMalloc_8u(drv_vm->m_nMemSize);
                if (NULL != drv_vm->m_pVideoMem) break;
            }
            if (i < min_b)
            {
                ERR_SET(VM_OPERATION_FAILED, "allocate video memory");
            }
            break;
        default:
            ERR_SET(VM_OPERATION_FAILED, "wrong video memory type specified");
            break;
        }; // switch (video_mem_type)
    }
    if (VM_OK == result)
    {
        if ((VideoDrvVideoMemInternal == video_mem_type) ||
            (VideoDrvVideoMemExternal == video_mem_type))
        {
            /* No video memory specified. Trying create buffers one by one. */
            bufs[0] = umc_vdrv_AlignPointer(drv_vm->m_pVideoMem,
                                            drv_vm->m_nAlignment);
            j = (drv_vm->m_nMemSize - ((Ipp8u*)bufs[0]-(Ipp8u*)drv_vm->m_pVideoMem)) / drv_vm->m_nBufSize;
            for (i = 1; i < j; ++i)
            {
                bufs[i] = ((Ipp8u*)bufs[i-1]) + drv_vm->m_nBufSize;
                if (NULL == bufs[i]) break;
            }
            if ((0 == j) || (i < min_b))
            {
                ERR_SET(VM_OPERATION_FAILED, "umc_x_CreateSurface");
            }
        }
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* void */
VIDEO_DRV_FREE_BUFFERS_FUNC(umc_vdrv_FreeBuffers, driver, num_b, bufs)
{
#undef  FUNCTION
#define FUNCTION "umc_vdrv_FreeBuffers"
    vm_status   result  = VM_OK;
    Ipp32u      i;

    DBG_SET("+");
    if (VM_OK == result)
    {
        if ((NULL == driver) || (NULL == bufs))
        {
            ERR_SET(VM_NULL_PTR, "null ptr");
        }
    }
    if (VM_OK == result)
    {
        switch (driver->m_VideoMemType)
        {
        case VideoDrvVideoMemLibrary:
            /* Memory was allocated by graphical library. Staring de-allocation. */
            for (i = 0; i < num_b; ++i)
            {
                driver->m_DrvSpec.drvFreeSurface(driver, bufs[i]);
                bufs[i] = NULL;
            }
            break;
        case VideoDrvVideoMemInternal:
            /* Memory was allocated interanaly, freeing. */
            ippsFree(driver->m_VideoMemInfo.m_pVideoMem);
            driver->m_VideoMemInfo.m_pVideoMem  = NULL;
            driver->m_VideoMemInfo.m_nMemSize   = 0;
            for (i = 0; i < num_b; ++i) bufs[i] = NULL;
            break;
        default:
        case VideoDrvVideoMemExternal:
            /* Driver used externally allocated memory.
             * Skipping de-allocation step.
             */
            break;
        }; // switch (driver->m_VideoMemType)
    }
    DBG_SET("-");
    return;
}

/* -------------------------------------------------------------------------- */

/* void* */
VIDEO_DRV_CREATE_SURFACE_FUNC(umc_vdrv_CreateSurface_ipps, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_vdrv_CreateSurface_ipps"
    void*   alloc_srf   = NULL;

    DBG_SET("+");
    alloc_srf = ippsMalloc_8u(driver->m_VideoMemInfo.m_nBufSize);
    DBG_SET("-");
    return alloc_srf;
}

/* -------------------------------------------------------------------------- */

/* void */
VIDEO_DRV_FREE_SURFACE_FUNC(umc_vdrv_FreeSurface_ipps, driver, srf)
{
#undef  FUNCTION
#define FUNCTION "umc_vdrv_FreeSurface_ipps"
    DBG_SET("+");

    if ((NULL != srf) && (driver != NULL)) 
      ippsFree(srf);
    DBG_SET("-");
    return;
}

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_LOCK_SURFACE_FUNC(umc_vdrv_LockSurface_rgb24,
                            driver, srf, planes)
{
#undef  FUNCTION
#define FUNCTION "umc_vdrv_LockSurface_rgb24"
    vm_status   result  = VM_OK;
    Ipp32s      i;

    DBG_SET("+");
    if (VM_OK == result)
    {
        planes[0].m_pPlane      = srf;
        planes[0].m_nPitch      = 3*driver->img_width;
        planes[0].m_nMemSize    = planes[0].m_nPitch * driver->img_width;
        for (i = 1; i < VIDEO_DRV_MAX_PLANE_NUMBER; ++i)
        {
            planes[i].m_pPlane      = NULL;
            planes[i].m_nPitch      = 0;
            planes[i].m_nMemSize    = 0;
        }
    }
    DBG_SET("-");
    return result;
}
