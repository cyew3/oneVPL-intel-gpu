//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2010 Intel Corporation. All Rights Reserved.
//

#include <umc_defs.h>

#ifndef LINUX32
#pragma warning(disable: 4100)
#endif

#ifdef UMC_ENABLE_FILE_WRITER

/* -------------------------------------------------------------------------- */

#include "null_drv.h"

#include <stdio.h>
#include <ipps.h>

/* -------------------------------------------------------------------------- */

#define MODULE              "NULL(video driver)"
#define FUNCTION            "<Undefined Function>"
#define ERR_STRING(msg)     VIDEO_DRV_ERR_STRING(MODULE, FUNCTION, msg)
#define ERR_SET(err, msg)   VIDEO_DRV_ERR_SET   (MODULE, err, FUNCTION, msg)
#define WRN_SET(err, msg)   VIDEO_DRV_WRN_SET   (MODULE, err, FUNCTION, msg)
#define DBG_SET(msg)        VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

/* -------------------------------------------------------------------------- */

const VideoDrvSpec NULLVideoDrvSpec =
{
    VM_STRING(MODULE),
    VideoDrvFalse,
    VideoDrvVideoMemInternal | VideoDrvVideoMemExternal,
    umc_null_Init,
    NULL, /*umc_null_Close, // - this function is unneeded now*/
    umc_vdrv_CreateBuffers,
    umc_vdrv_FreeBuffers,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    umc_null_GetWindow,
    NULL
};

/* -------------------------------------------------------------------------- */

/* vm_status */
VIDEO_DRV_INIT_FUNC (umc_null_Init, driver, params)
{
#undef  FUNCTION
#define FUNCTION "umc_null_Init"
    vm_status           result  = VM_OK;
    NULLVideoDrvParams* pParams = (NULLVideoDrvParams*) params;

    DBG_SET("+");
    if (result == VM_OK)
    {
        if ((NULL == driver) || (NULL == pParams))
        {
            ERR_SET(VM_NULL_PTR,"null ptr");
        }
    }
    if (result == VM_OK)
    {
        driver->m_pDrv          = NULL;
        driver->img_width       = pParams->common.img_width;
        driver->img_height      = pParams->common.img_height;
        driver->m_VideoMemType  = VideoDrvVideoMemInternal;
        ippsZero_8u((Ipp8u*)&(driver->m_VideoMemInfo), sizeof(VideoDrvVideoMemInfo));
        ippsCopy_8u((const Ipp8u*)&NULLVideoDrvSpec, (Ipp8u*)&(driver->m_DrvSpec), sizeof(VideoDrvSpec));
    }
    DBG_SET("-");
    return result;
}

/* -------------------------------------------------------------------------- */

/* void */
VIDEO_DRV_CLOSE_FUNC(umc_null_Close, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_null_Close"
    DBG_SET("+");
    DBG_SET("-");
    return;
}

/* -------------------------------------------------------------------------- */

/* void* */
VIDEO_DRV_GET_WINDOW_FUNC(umc_null_GetWindow, driver)
{
#undef  FUNCTION
#define FUNCTION "umc_null_GetWindow"
    void*   win = NULL;

    DBG_SET("+");
    DBG_SET("-");
    return win;
}

/* -------------------------------------------------------------------------- */

#endif /* defined(UMC_ENABLE_FILE_WRITER) */
