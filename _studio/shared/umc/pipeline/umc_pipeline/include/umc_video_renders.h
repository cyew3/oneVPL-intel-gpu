//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VIDEO_RENDERS_H__
#define __UMC_VIDEO_RENDERS_H__

#include "umc_defs.h"

#if defined(UMC_ENABLE_BLT_VIDEO_RENDER)
#include "blt_video_render.h"
#endif

#if defined(UMC_ENABLE_DX_VIDEO_RENDER)
#include "dx_video_render.h"
#endif

#if defined(UMC_ENABLE_SDL_VIDEO_RENDER)
#include "sdl_video_render.h"
#endif

#include "null_video_renderer.h"
#include "fw_video_render.h"

#endif // __UMC_VIDEO_RENDERS_H__

