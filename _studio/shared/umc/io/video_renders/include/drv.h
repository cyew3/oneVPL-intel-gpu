//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2010 Intel Corporation. All Rights Reserved.
//

/* This is main header file for all UMC video drivers. */

#ifndef __DRV_H__
#define __DRV_H__

#include <vm_types.h>
#include <vm_debug.h>
//
// uncomment the line below or define VIDEO_DRV_USE_DEBUG to obtain additional debug output
//#define VIDEO_DRV_USE_DEBUG

typedef enum VideoDrvBool
{
    VideoDrvFalse = 0, VideoDrvTrue = 1
} VideoDrvBool;

typedef enum VideoDrvVideoMode
{
    VideoDrvOrdinary = 0, VideoDrvFullScreen = 1
} VideoDrvVideoMode;

/* Name: VideoDrvRect.
 *
 * Purpose:
 *  Store geometry information about rectangle on the screen.
 *
 * Structure fields:
 *  x,y - position of the upper left coner of the rectangle;
 *  w,h - rectangle sizes: width and height.
 */

typedef struct VideoDrvRect
{
    Ipp32s x;  Ipp32s y;
    Ipp32s w;  Ipp32s h;
} VideoDrvRect;

/* Name: VideoDrvParams.
 *
 * Purpose:
 *  Store common parameters needed for all drivers initialization.
 *
 * Structure fields:
 *  win_rect    - window rectangle (user recomended rectangle, driver can
 * modify provided parameters if specified rectangle can't be displayed on the
 * screen for some reason).
 */

typedef struct VideoDrvParams
{
    VideoDrvRect    win_rect;
    Ipp32s          img_width;
    Ipp32s          img_height;
} VideoDrvParams;

#define VIDEO_DRV_MAX_PLANE_NUMBER  4
/* Memory alignment constants. */
#define VIDEO_DRV_ALIGN_BYTE        1 /* No alignment*/
#define VIDEO_DRV_ALIGN_EVEN_BYTES  2
#define VIDEO_DRV_ALIGN_WORD        4
#define VIDEO_DRV_ALIGN_DWORD       8

#define VIDEO_DRV_ALIGN_DEFAULT     VIDEO_DRV_ALIGN_BYTE
#define VIDEO_DRV_ALIGN_IGNORE      VIDEO_DRV_ALIGN_BYTE /* To ignore align. */

typedef struct VideoDrvPlaneInfo
{
    void*       m_pPlane;
    size_t    m_nPitch;
    size_t    m_nMemSize;
} VideoDrvPlaneInfo;

typedef enum VideoDrvVideoMemType
{
    VideoDrvVideoMemInternal = 0x00000001,
    VideoDrvVideoMemExternal = 0x00000002,
    VideoDrvVideoMemLibrary  = 0x00000004
} VideoDrvVideoMemType;

/* Name: VideoDrvVideoMemInfo.
 *
 * Purpose:
 *  Store information about video memory (this memory can be devided to a
 * number of video buffers).
 *
 * Structure fields:
 *  m_pVideoMem         - pointer to video memory;
 *  m_nMemSize          - video memory size;
 *  m_nPointerAlignment - pointer alignment (buffers alignment);
 *  m_nPitchAlignment   - pitch alignment (data alignment);
 *  m_nPitch            - pitch;
 *  m_nBufSize          - size of one buffer;
 */

typedef struct VideoDrvVideoMemInfo
{
    void*       m_pVideoMem;
    size_t    m_nMemSize;

    size_t    m_nAlignment;
    size_t    m_nBufSize;
    size_t    m_nPitch;
} VideoDrvVideoMemInfo;

/* Macroses defined below specifies video driver functions prototypes.
 *
 * Agreements:
 *  0. Names of macroses: VIDEO_DRV_<name>_FUNC;
 *  1. func     - first parameter of all macroses, function name;
 *  2. driver   - first parameter of almost all functions, pointer to the driver
 * specification;
 *  3. below only function-specific parameters and return values will be
 * described.
 */

/* See declaration of <struct VideoDriver> below. */
typedef struct VideoDriver VideoDriver;

/* Name: VIDEO_DRV_INIT_FUNC.
 *
 * Purpose:
 *  Initialize video driver.
 *
 * Parameters:
 *  driver  - should consist driver specification, field driver->m_pDrv is
 *  function return value;
 *  params  - parameters needed for driver initialization.
 */

#define VIDEO_DRV_INIT_FUNC(func, driver, params)                        \
    vm_status func(VideoDriver *driver, void *params)

/* Name: VIDEO_DRV_CLOSE_FUNC.
 *
 * Purpose:
 *  Close/deinitialize driver.
 */

#define VIDEO_DRV_CLOSE_FUNC(func, driver)                               \
    void func(VideoDriver *driver)

/* Name: VIDEO_DRV_CREATE_BUFFERS_FUNC.
 *
 * Purpose:
 *  Initialize driver-specific buffers (overlay, surface or something else)
 * to store video data.
 *
 * Parameters:
 *  min_b   - minimum number of buffers to initialize;
 *  max_b   - maximim number of buffers to initialize;
 *  bufs    - pointer to the list of buffers (return values), number of
 * elements in the list - at least max_b elements;
 *  video_mem_info  - video memory information (can be NULL if graphical
 * library will use internal buffers).
 *
 * Remarks:
 *  1. video_mem_info can provide reference to a pre-allocated memory;
 *  2. video_mem_info can provide only buffers information (buffer size,
 *  pitch).
 */

#define VIDEO_DRV_CREATE_BUFFERS_FUNC(func,                              \
                                      driver, min_b, max_b, bufs,        \
                                      video_mem_type, video_mem_info)    \
    Ipp32u func(VideoDriver *driver,                                     \
                Ipp32u min_b, Ipp32u max_b,                              \
                void **bufs,                                             \
                VideoDrvVideoMemType    video_mem_type,                  \
                VideoDrvVideoMemInfo*   video_mem_info)

/* Name: VIDEO_DRV_FREE_BUFFERS_FUNC.
 *
 * Purpose:
 *  Destroy driver-specific buffers.
 *
 * Parameters:
 *  num_b   - number of buffers;
 *  bufs    - pointer to the list of buffers (return values), number of
 * elements in the list - num_b elements;
 */

#define VIDEO_DRV_FREE_BUFFERS_FUNC(func,                                \
                                    driver, num_b, bufs)                 \
    void func(VideoDriver *driver, Ipp32u num_b, void **bufs)

/* Name: VIDEO_DRV_CREATE_SURFACE_FUNC.
 *
 * Purpose:
 *  Create driver-specific buffer (overlay, surface or something else)
 * to store video data.
 *
 * Return value:
 *  Pointer to the initialized buffer.
 */

#define VIDEO_DRV_CREATE_SURFACE_FUNC(func, driver)                      \
    void* func(VideoDriver *driver)

/* Name: VIDEO_DRV_FREE_SURFACE_FUNC.
 *
 * Purpose:
 *  Destroy driver-specific buffer (overlay, surface or something else)
 * previously created by VIDEO_DRV_CREATE_SURFACE_FUNC.
 *
 * Parameters:
 *  srf - buffer to free.
 */

#define VIDEO_DRV_FREE_SURFACE_FUNC(func, driver, srf)                   \
    void func(VideoDriver *driver, void *srf)

/* Name: VIDEO_DRV_LOCK_SURFACE_FUNC.
 *
 * Purpose:
 *  Lock driver-specific buffer and return some basic information about it.
 *
 * Parameters:
 *  srf     - buffer to lock;
 *  pixels  - planes of pixels, muximim number of planes - 3 (return value);
 *  pitches - pitches (return value);
 *  planes  - number of planes (return value).
 */

#define VIDEO_DRV_LOCK_SURFACE_FUNC(func, driver, srf, planes)           \
    vm_status func (VideoDriver *driver, void *srf,                      \
                    VideoDrvPlaneInfo planes[VIDEO_DRV_MAX_PLANE_NUMBER])

/* Name: VIDEO_DRV_UNLOCK_SURFACE_FUNC.
 *
 * Purpose:
 *  Unlock driver-specific buffer previously locked with
 * VIDEO_DRV_UNLOCK_SURFACE_FUNC.
 *
 * Parameters:
 *  srf     - buffer to unlock.
 */

#define VIDEO_DRV_UNLOCK_SURFACE_FUNC(func, driver, srf)                       \
    vm_status func(VideoDriver *driver, void *srf)

/* Name: VIDEO_DRV_RENDER_FRAME_FUNC.
 *
 * Purpose:
 *  Render video frame.
 *
 * Parameters:
 *  frame   - frame to render;
 *  rect    - position of the frame in the window (can be NULL).
 */

#define VIDEO_DRV_RENDER_FRAME_FUNC(func, driver, frame, rect)                 \
    vm_status func(VideoDriver *driver, void *frame, VideoDrvRect *rect)

/* Name: VIDEO_DRV_SET_VIDEO_MODE_FUNC.
 *
 * Purpose:
 *  initialize video regime with some parameters.
 *
 * Parameters:
 *  rect    - window rectangle;
 *  mode    - specifies video regime to use.
 */

#define VIDEO_DRV_SET_VIDEO_MODE_FUNC(func, driver, rect, mode)                \
    vm_status func(VideoDriver *driver, VideoDrvRect *rect, VideoDrvVideoMode mode)

/* Name: VIDEO_DRV_GET_WINDOW_FUNC.
 *
 * Purpose:
 *  Return descriptor of the window.
 *
 * Return value:
 *  Type of teturn variable depends on driver which was used.
 */

#define VIDEO_DRV_GET_WINDOW_FUNC(func, driver)                                \
    void* func(VideoDriver *driver)

/* Name: VIDEO_DRV_RUN_EVENT_LOOP_FUNC.
 *
 * Purpose:
 *  Run event loop.
 *
 * Remarks:
 *  1. Function doesn't run infinity loop. It checks and releases all events
 *  from queue and quits.
 */

#define VIDEO_DRV_RUN_EVENT_LOOP_FUNC(func, driver)                            \
    vm_status func(VideoDriver *driver)

/* Next types can be used to work with driver functions. */

typedef VIDEO_DRV_INIT_FUNC (VideoDrvInitFunc, driver, params);
typedef VIDEO_DRV_CLOSE_FUNC(VideoDrvCloseFunc, driver);

typedef VIDEO_DRV_CREATE_BUFFERS_FUNC(VideoDrvCreateBuffersFunc,
                                      driver, min_b, max_b, bufs,
                                      video_mem_type, video_mem_info);
typedef VIDEO_DRV_FREE_BUFFERS_FUNC(VideoDrvFreeBuffersFunc,
                                    driver, num_b, bufs);
typedef VIDEO_DRV_CREATE_SURFACE_FUNC(VideoDrvCreateSurfaceFunc, driver);
typedef VIDEO_DRV_FREE_SURFACE_FUNC(VideoDrvFreeSurfaceFunc, driver, srf);

typedef VIDEO_DRV_LOCK_SURFACE_FUNC(VideoDrvLockSurfaceFunc,
                                    driver, srf, planes);
typedef VIDEO_DRV_UNLOCK_SURFACE_FUNC(VideoDrvUnlockSurfaceFunc, driver, srf);

typedef VIDEO_DRV_RENDER_FRAME_FUNC(VideoDrvRenderFrameFunc,
                                    driver, frame, rect);
typedef VIDEO_DRV_SET_VIDEO_MODE_FUNC(VideoDrvSetVideoModeFunc,
                                      driver, rect, mode);

typedef VIDEO_DRV_GET_WINDOW_FUNC(VideoDrvGetWindowFunc, driver);
typedef VIDEO_DRV_RUN_EVENT_LOOP_FUNC(VideoDrvRunEventLoopFunc, driver);

/* Name: VideoDrvSpec.
 *
 * Purpose:
 *  Store information about driver, store pointers to driver functions.
 */

typedef struct VideoDrvSpec
{
    vm_char*                    drv_name;
    VideoDrvBool                drv_rebuf;
    Ipp32u                      drv_videomem_type;  /*VideoDrvVideoMemType*/

    VideoDrvInitFunc*           drvInit;
    VideoDrvCloseFunc*          drvClose;

    VideoDrvCreateBuffersFunc*  drvCreateBuffers;
    VideoDrvFreeBuffersFunc*    drvFreeBuffers;
    VideoDrvCreateSurfaceFunc*  drvCreateSurface;   /* usually unused directly */
    VideoDrvFreeSurfaceFunc*    drvFreeSurface;     /* usually unused directly */

    VideoDrvLockSurfaceFunc*    drvLockSurface;
    VideoDrvUnlockSurfaceFunc*  drvUnlockSurface;

    VideoDrvRenderFrameFunc*    drvRenderFrame;
    VideoDrvSetVideoModeFunc*   drvSetVideoMode;

    VideoDrvGetWindowFunc*      drvGetWindow;
    VideoDrvRunEventLoopFunc*   drvRunEventLoop;
} VideoDrvSpec;

/* Name: VideoDrvSpec.
 *
 * Purpose:
 *  Store driver specification.
 *
 * Structure fields:
 *  img_width, img_height   - original image sizes;
 *  m_pVideoMemInfo         - video memory information (optional parameter);
 *  m_DrvSpec               - driver functions;
 *  m_pDrv                  - specific driver information.
 */

struct VideoDriver
{
    Ipp32s                  img_width;
    Ipp32s                  img_height;

    VideoDrvVideoMemType    m_VideoMemType;
    VideoDrvVideoMemInfo    m_VideoMemInfo;

    VideoDrvSpec            m_DrvSpec;
    void*                   m_pDrv;
};

/* Macroses defined below can be used for errors handling in drivers code.
 *
 * Agreements:
 *  0. Names of macroses: VIDEO_DRV_<name>;
 *  1. module   - macroses parameter, string containing module name;
 *  2. func     - macroses parameter, string containing function name;
 *  3. msg      - macroses parameter, message specifies an error occured;
 *  4. below only macroses-specific parameters values will be described.
 */

/* Name: VIDEO_DRV_RESULT.
 *
 * Purpose:
 *  Defines result variable for macroses below.
 */
#define VIDEO_DRV_RESULT result

/* Name: VIDEO_DRV_FUNC_SET.
 *
 * Purpose:
 *  Concatinate module name and file name into full file name.
 */

#define VIDEO_DRV_FUNC_SET(module, func)                                       \
    VM_STRING(module func)

/* Name: VIDEO_DRV_RESULT_SET.
 *
 * Purpose:
 *  Set <result> variable, defined by user.
 *
 * Parameters:
 *  level   - level of an error (error, warning, ...);
 *  err     - error occured.
 *
 * Remarks:
 *  1. <result> variable should be defined before usage of this macro;
 *  2. <err> and <result> should have same tpes.
 */

#ifdef VIDEO_DRV_USE_DEBUG
#  define VIDEO_DRV_RESULT_SET(level, module, err, func, msg)                            \
{                                                                                        \
    {                                                                                    \
        /*vm_debug_trace_withfunc(level,                                                 \
                                VIDEO_DRV_FUNC_SET(module, func),                        \
                                msg);*/                                                  \
        /*printf ("%s: %s\n", VIDEO_DRV_FUNC_SET(module, func), msg);*/                  \
    }                                                                                    \
    VIDEO_DRV_RESULT = err;                                                              \
}
#else
#  define VIDEO_DRV_RESULT_SET(level, module, err, func, msg) VIDEO_DRV_RESULT = err;
#endif

/* Name: VIDEO_DRV_ERR_SET.
 *
 * Purpose:
 *  Set <result> variable, defined by user (use this macro to report about
 * errors).
 *
 * Parameters:
 *  err     - error occured.
 *
 * Remarks:
 *  1. <result> variable should be defined before usage of this macro;
 *  2. <err> and <result> should have same tpes.
 */

#define VIDEO_DRV_ERR_SET(module, err, func, msg) \
    VIDEO_DRV_RESULT_SET(VM_DEBUG_ERROR, module, err, func, msg)

/* Name: VIDEO_DRV_WRN_SET.
 *
 * Purpose:
 *  Set <result> variable, defined by user (use this macro to report about
 * warnings).
 *
 * Parameters:
 *  err     - error occured.
 *
 * Remarks:
 *  1. <result> variable should be defined before usage of this macro;
 *  2. <err> and <result> should have same tpes.
 */

#define VIDEO_DRV_WRN_SET(module, err, func, msg) \
    VIDEO_DRV_RESULT_SET(VM_DEBUG_WARNING, module, err, func, msg)

/* Name: VIDEO_DRV_DBG_SET.
 *
 * Purpose:
 *  Print debug message (message for information purposes only).
 */

#if (0 < VIDEO_DRV_USE_DEBUG)
#define VIDEO_DRV_DBG_SET(module, func, msg)                                   \
{                                                                              \
    {                                                                          \
        vm_debug_trace_withfunc(VM_DEBUG_PROGRESS,                             \
                                VIDEO_DRV_FUNC_SET(module, func),              \
                                msg);                                         \
    }                                                                          \
}
#else /* !(0 < VIDEO_DRV_USE_DEBUG) */
#define VIDEO_DRV_DBG_SET(module, func, msg)
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* Memory align function. */
    size_t umc_vdrv_AlignValue(size_t nValue, size_t lAlign);

    /* Init buffers functions. */
    extern VIDEO_DRV_CREATE_BUFFERS_FUNC(umc_vdrv_CreateBuffers,
                                         driver, min_b, max_b, bufs,
                                         video_mem_type, video_mem_info);

    /* Free buffers functions. */
    extern VIDEO_DRV_FREE_BUFFERS_FUNC(umc_vdrv_FreeBuffers,
                                       driver, num_b, bufs);

    /* Create surface functions for different formats. */
    extern VIDEO_DRV_CREATE_SURFACE_FUNC(umc_vdrv_CreateSurface_ipps,
                                         driver);

    /* Free surface functions for different formats. */
    extern VIDEO_DRV_FREE_SURFACE_FUNC(umc_vdrv_FreeSurface_ipps,
                                       driver,srf);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DRV_H__ */
