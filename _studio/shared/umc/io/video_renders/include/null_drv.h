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

#ifdef UMC_ENABLE_FILE_WRITER

#ifndef _NULL_DRV_H_
#define _NULL_DRV_H_

#include "drv.h"

typedef struct NULLVideoDrvParams
{
    VideoDrvParams  common;     /* common parameters */
} NULLVideoDrvParams;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    extern VIDEO_DRV_INIT_FUNC(umc_null_Init, driver, params);
    extern VIDEO_DRV_CLOSE_FUNC(umc_null_Close, driver);

    extern VIDEO_DRV_RENDER_FRAME_FUNC(umc_null_RenderFrame, driver, frame, rect);

    /* Function always returns NULL. */
    extern VIDEO_DRV_GET_WINDOW_FUNC(umc_null_GetWindow, driver);

    extern const VideoDrvSpec NULLVideoDrvSpec;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _NULL_DRV_H_ */
#endif /* defined(UMC_ENABLE_FILE_WRITER) */
