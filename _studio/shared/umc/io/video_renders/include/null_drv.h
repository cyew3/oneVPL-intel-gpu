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
