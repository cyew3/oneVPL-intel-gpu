//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_AUDIO_RENDERS_H__
#define __UMC_AUDIO_RENDERS_H__

#ifdef _WIN32_WINNT
#include "directsound_render.h"
#endif    // _WIN32_WINNT

#ifdef LINUX32
#ifndef OSX32
#include "oss_audio_render.h"
#endif // OSX32
#endif // LINUX32

#include "null_audio_render.h"
#include "fw_audio_render.h"

#endif // __UMC_AUDIO_RENDERS_H__
