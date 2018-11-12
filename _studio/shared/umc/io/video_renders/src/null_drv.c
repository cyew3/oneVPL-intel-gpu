// Copyright (c) 2001-2018 Intel Corporation
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
